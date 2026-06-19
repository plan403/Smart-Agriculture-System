/**
 * @file    unit_tests.c
 * @brief   单元测试用例
 * @note    在PC端使用 GCC 编译运行，验证算法逻辑
 *          编译: gcc -o unit_tests unit_tests.c ../src/utils/data_filter.c ../src/utils/crc_check.c -I../include -I../config
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

/* 模拟嵌入式环境类型定义 */
#include "common_types.h"
#include "error_codes.h"

/* 外部测试对象 */
extern float data_filter_sliding_window(float new_value, uint8_t sensor_id);
extern float data_filter_median(float *data, uint8_t len);
extern float data_filter_low_pass(float current, float new_val, float alpha);
extern float data_filter_limit(float new_val, float last_val, float max_diff);
extern void  data_filter_reset_all(void);
extern uint16_t crc16_calculate(uint8_t *data, uint16_t len);
extern uint8_t  crc8_calculate(uint8_t *data, uint16_t len);
extern const char* error_to_string(uint8_t err_code);

/* ================================================================
 * 测试辅助宏
 * ================================================================ */
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s (line %d): %s\n", __func__, __LINE__, msg); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_FLOAT_EQ(a, b, eps, msg) do { \
    if (fabsf((a) - (b)) > (eps)) { \
        printf("  [FAIL] %s (line %d): %s (expected %.4f, got %.4f)\n", \
               __func__, __LINE__, msg, (float)(b), (float)(a)); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_START(name) printf("\n[TEST] %s\n", name)
#define TEST_PASS() g_tests_passed++

/* ================================================================
 * 数据滤波测试
 * ================================================================ */

void test_sliding_window_basic(void)
{
    TEST_START("滑动窗口滤波 - 基本功能");
    data_filter_reset_all();

    /* 输入稳定数据，输出应接近输入 */
    float result;
    for (int i = 0; i < 20; i++) {
        result = data_filter_sliding_window(25.0f, 0);
    }
    TEST_ASSERT_FLOAT_EQ(result, 25.0f, 0.5f, "稳定输入输出应接近");
    TEST_PASS();
}

void test_sliding_window_noise_reduction(void)
{
    TEST_START("滑动窗口滤波 - 噪声抑制");
    data_filter_reset_all();

    /* 输入带噪声的数据 */
    data_filter_sliding_window(25.0f, 1);
    data_filter_sliding_window(25.1f, 1);
    data_filter_sliding_window(24.9f, 1);
    data_filter_sliding_window(25.0f, 1);
    data_filter_sliding_window(99.0f, 1);  /* 异常值 */
    data_filter_sliding_window(25.1f, 1);
    data_filter_sliding_window(24.8f, 1);
    float result = data_filter_sliding_window(25.0f, 1);

    /* 剔除极值后，结果应在25附近 */
    TEST_ASSERT_FLOAT_EQ(result, 25.0f, 2.0f, "应滤除异常峰值");
    TEST_PASS();
}

void test_median_filter(void)
{
    TEST_START("中位值滤波");
    float data[] = {1.0f, 5.0f, 3.0f, 99.0f, 2.0f};
    float result = data_filter_median(data, 5);

    TEST_ASSERT_FLOAT_EQ(result, 3.0f, 0.01f, "中位值应为3.0");
    TEST_PASS();
}

void test_low_pass_filter(void)
{
    TEST_START("一阶低通滤波");
    float result = 0.0f;

    /* alpha=0.3 表示新值权重30% */
    for (int i = 0; i < 10; i++) {
        result = data_filter_low_pass(result, 100.0f, 0.3f);
    }
    /* 经过多次迭代应接近100 */
    TEST_ASSERT(result > 90.0f, "低通滤波应趋向目标值");
    TEST_PASS();
}

void test_limit_filter(void)
{
    TEST_START("限幅滤波");

    /* 变化在阈值内，采用新值 */
    float result = data_filter_limit(25.5f, 25.0f, 1.0f);
    TEST_ASSERT_FLOAT_EQ(result, 25.5f, 0.01f, "变化在阈值内应采用新值");

    /* 变化超过阈值，保持旧值 */
    result = data_filter_limit(30.0f, 25.0f, 1.0f);
    TEST_ASSERT_FLOAT_EQ(result, 25.0f, 0.01f, "变化超阈值应保持旧值");

    TEST_PASS();
}

/* ================================================================
 * CRC 校验测试
 * ================================================================ */

void test_crc16_known_value(void)
{
    TEST_START("CRC16 已知值验证");
    uint8_t test_data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint16_t crc = crc16_calculate(test_data, sizeof(test_data));

    /* CRC16-CCITT of "123456789" = 0x29B1 */
    TEST_ASSERT(crc == 0x29B1, "CRC16('123456789') 应为 0x29B1");
    TEST_PASS();
}

void test_crc16_empty(void)
{
    TEST_START("CRC16 空数据");
    uint16_t crc = crc16_calculate(NULL, 0);
    TEST_ASSERT(crc == 0xFFFF, "空数据CRC应为初始值0xFFFF");
    TEST_PASS();
}

void test_crc8_dallas(void)
{
    TEST_START("CRC8 Maxim 测试");
    uint8_t test_data[] = {0x00, 0x00, 0x00};
    uint8_t crc = crc8_calculate(test_data, sizeof(test_data));

    /* 全0数据的CRC8应为0x00 */
    TEST_ASSERT(crc == 0x00, "CRC8(0,0,0) 应为 0x00");
    TEST_PASS();
}

/* ================================================================
 * 错误码测试
 * ================================================================ */

void test_error_to_string(void)
{
    TEST_START("错误码转字符串");
    TEST_ASSERT(strcmp(error_to_string(ERR_OK), "OK") == 0, "ERR_OK -> OK");
    TEST_ASSERT(strcmp(error_to_string(ERR_TIMEOUT), "Timeout") == 0, "ERR_TIMEOUT -> Timeout");
    TEST_ASSERT(strcmp(error_to_string(ERR_I2C_NACK), "I2C NACK") == 0, "ERR_I2C_NACK -> I2C NACK");
    TEST_ASSERT(strcmp(error_to_string(0xFF), "Unknown Error") == 0, "未知 -> Unknown Error");
    TEST_PASS();
}

/* ================================================================
 * 环形缓冲区测试
 * ================================================================ */

/* (声明外部函数) */
extern void ring_buffer_init(void *rb, uint8_t *buf, uint16_t size);
extern bool ring_buffer_put(void *rb, uint8_t data);
extern bool ring_buffer_get(void *rb, uint8_t *data);
extern uint16_t ring_buffer_count(void *rb);
extern bool ring_buffer_is_empty(void *rb);
extern bool ring_buffer_is_full(void *rb);
extern void ring_buffer_clear(void *rb);

void test_ring_buffer_basic(void)
{
    TEST_START("环形缓冲区 - 基本读写");
    uint8_t buf[16];
    uint8_t rb_struct[64];  /* 模拟 ring_buffer_t */
    uint8_t data;

    ring_buffer_init(rb_struct, buf, 16);

    TEST_ASSERT(ring_buffer_is_empty(rb_struct), "初始应为空");
    TEST_ASSERT(ring_buffer_put(rb_struct, 'A'), "写入应成功");
    TEST_ASSERT(!ring_buffer_is_empty(rb_struct), "写入后不应为空");
    TEST_ASSERT(ring_buffer_count(rb_struct) == 1, "应有1个元素");

    TEST_ASSERT(ring_buffer_get(rb_struct, &data), "读取应成功");
    TEST_ASSERT(data == 'A', "读取值应为'A'");
    TEST_ASSERT(ring_buffer_is_empty(rb_struct), "读空后应为空");

    TEST_PASS();
}

void test_ring_buffer_full(void)
{
    TEST_START("环形缓冲区 - 满检测");
    uint8_t buf[4];
    uint8_t rb_struct[64];

    ring_buffer_init(rb_struct, buf, 4);

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(ring_buffer_put(rb_struct, (uint8_t)i), "写入应成功");
    }
    TEST_ASSERT(ring_buffer_is_full(rb_struct), "缓冲区应满");
    TEST_ASSERT(!ring_buffer_put(rb_struct, 100), "满时写入应失败");

    TEST_PASS();
}

/* ================================================================
 * 主函数
 * ================================================================ */

int main(void)
{
    printf("============================================\n");
    printf("  SmartAgricultureOS 单元测试\n");
    printf("============================================\n");

    /* 数据滤波 */
    test_sliding_window_basic();
    test_sliding_window_noise_reduction();
    test_median_filter();
    test_low_pass_filter();
    test_limit_filter();

    /* CRC校验 */
    test_crc16_known_value();
    test_crc16_empty();
    test_crc8_dallas();

    /* 错误码 */
    test_error_to_string();

    /* 环形缓冲区 */
    test_ring_buffer_basic();
    test_ring_buffer_full();

    printf("\n============================================\n");
    printf("  测试结果: %d 通过, %d 失败\n",
           g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
