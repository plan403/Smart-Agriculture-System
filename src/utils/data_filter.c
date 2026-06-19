/**
 * @file    data_filter.c
 * @brief   数据滤波算法
 * @note    实现均值滤波、去极值算法、滑动窗口
 *          消除环境干扰导致的数据跳动误差
 */

#include "system_config.h"
#include "common_types.h"

/* ================================================================
 * 滑动窗口滤波器
 * 每个传感器维护独立的历史数据窗口
 * ================================================================ */
#define MAX_SENSORS     8
#define WINDOW_SIZE     FILTER_WINDOW_SIZE

typedef struct {
    float   buffer[WINDOW_SIZE];         /* 数据窗口 */
    uint8_t index;                       /* 当前写入位置 */
    uint8_t count;                       /* 已写入数据量 */
    uint8_t initialized;                 /* 是否已填满窗口 */
} sliding_window_t;

static sliding_window_t g_windows[MAX_SENSORS];

/**
 * @brief   滑动均值滤波
 * @param   new_value: 新的采样值
 * @param   sensor_id: 传感器编号 (用于选择对应窗口)
 * @return  滤波后的值
 * @note    先剔除窗口内的极值(最大/最小各N个)，再取平均
 */
float data_filter_sliding_window(float new_value, uint8_t sensor_id)
{
    if (sensor_id >= MAX_SENSORS) return new_value;

    sliding_window_t *win = &g_windows[sensor_id];

    /* 初始化窗口 */
    if (!win->initialized) {
        for (uint8_t i = 0; i < WINDOW_SIZE; i++) {
            win->buffer[i] = new_value;
        }
        win->index = 0;
        win->count = WINDOW_SIZE;
        win->initialized = 1;
        return new_value;
    }

    /* 写入新值 (环形覆盖) */
    win->buffer[win->index] = new_value;
    win->index = (win->index + 1) % WINDOW_SIZE;
    if (win->count < WINDOW_SIZE) win->count++;

    /* ---- 去极值均值滤波 ---- */
    /* 1. 复制窗口数据进行排序 */
    float sorted[WINDOW_SIZE];
    for (uint8_t i = 0; i < win->count; i++) {
        sorted[i] = win->buffer[i];
    }

    /* 2. 简单冒泡排序 (数据量小，开销可控) */
    for (uint8_t i = 0; i < win->count - 1; i++) {
        for (uint8_t j = i + 1; j < win->count; j++) {
            if (sorted[i] > sorted[j]) {
                float tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    /* 3. 剔除两端极值后取平均 */
    uint8_t start = FILTER_EXTREME_COUNT;
    uint8_t end = win->count - FILTER_EXTREME_COUNT;

    if (end <= start) {
        /* 数据量不足，直接取中位数 */
        return sorted[win->count / 2];
    }

    float sum = 0.0f;
    uint8_t valid_count = 0;
    for (uint8_t i = start; i < end; i++) {
        sum += sorted[i];
        valid_count++;
    }

    return sum / valid_count;
}

/**
 * @brief   一阶低通滤波 (补充滤波)
 * @param   current: 当前滤波值
 * @param   new_val: 新采样值
 * @param   alpha:   滤波系数 (0.0~1.0, 越小越平滑)
 * @return  滤波结果
 */
float data_filter_low_pass(float current, float new_val, float alpha)
{
    return alpha * new_val + (1.0f - alpha) * current;
}

/**
 * @brief   中位值滤波
 * @param   data: 数据数组
 * @param   len:  数据长度
 * @return  中位值
 */
float data_filter_median(float *data, uint8_t len)
{
    if (len == 0) return 0.0f;
    if (len == 1) return data[0];

    /* 排序取中位 */
    float sorted[16];  /* 最多处理16个数据 */
    if (len > 16) len = 16;

    for (uint8_t i = 0; i < len; i++) sorted[i] = data[i];

    for (uint8_t i = 0; i < len - 1; i++) {
        for (uint8_t j = i + 1; j < len; j++) {
            if (sorted[i] > sorted[j]) {
                float tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    return sorted[len / 2];
}

/**
 * @brief   限幅滤波
 * @param   new_val:  新采样值
 * @param   last_val: 上次有效值
 * @param   max_diff: 允许的最大变化量
 * @return  有效值 (超过限幅则返回last_val)
 */
float data_filter_limit(float new_val, float last_val, float max_diff)
{
    float diff = new_val - last_val;
    if (diff < 0) diff = -diff;

    if (diff > max_diff) {
        return last_val;  /* 变化过大，视为干扰 */
    }
    return new_val;
}

/**
 * @brief   重置指定传感器的滤波窗口
 */
void data_filter_reset(uint8_t sensor_id)
{
    if (sensor_id < MAX_SENSORS) {
        g_windows[sensor_id].initialized = 0;
        g_windows[sensor_id].count = 0;
        g_windows[sensor_id].index = 0;
    }
}

/**
 * @brief   重置全部滤波窗口
 */
void data_filter_reset_all(void)
{
    for (uint8_t i = 0; i < MAX_SENSORS; i++) {
        data_filter_reset(i);
    }
}
