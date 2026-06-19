/**
 * @file    sensor_task.c
 * @brief   环境采集任务 - 周期性采集所有传感器数据
 * @note    优先级最低，但周期最固定
 *          通过信号量保护数据，通过消息队列分发数据
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"
#include "pin_config.h"

/* 外部驱动接口 */
extern int i2c_read_bytes(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);
extern int spi_read_bytes(uint8_t cs_id, uint8_t *data, uint16_t len);
extern os_sem_t* get_sem_sensor_data(void);
extern os_queue_t* get_queue_sensor(void);
extern os_queue_t* get_queue_alarm(void);

/* 外部工具函数 */
extern float data_filter_sliding_window(float new_value, uint8_t sensor_id);
extern uint32_t os_get_tick(void);
extern void os_delay_ms(uint32_t ms);

/* ================================================================
 * 传感器数据结构
 * ================================================================ */
static env_data_t g_env_data;               /* 当前环境数据 */
static uint8_t    g_sensor_online_mask = 0x0F; /* 传感器在线状态掩码 */

/* ================================================================
 * 传感器读取函数
 * ================================================================ */

/**
 * @brief   读取 SHT30 温湿度传感器
 * @note    I2C地址 0x44，单次测量模式
 *          返回 6 字节: Temp_MSB, Temp_LSB, Temp_CRC, Humi_MSB, Humi_LSB, Humi_CRC
 */
static int read_temp_humi(float *temp, float *humi)
{
    uint8_t buf[6];
    int ret;

    /* 发送测量命令 (高重复性, 时钟拉伸使能) */
    uint8_t cmd[] = {0x2C, 0x06};
    ret = i2c_write_bytes(I2C_ADDR_TEMP_HUMI, cmd[0], &cmd[1], 1);
    if (ret != ERR_OK) return ret;

    os_delay_ms(20);  /* 等待测量完成 (高重复性 ~15ms) */

    /* 读取6字节数据 */
    ret = i2c_read_bytes(I2C_ADDR_TEMP_HUMI, 0x00, buf, 6);
    if (ret != ERR_OK) return ret;

    /* 解析温度: T = -45 + 175 * (S_T / (2^16 - 1)) */
    uint16_t raw_temp = ((uint16_t)buf[0] << 8) | buf[1];
    *temp = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);

    /* 解析湿度: RH = 100 * (S_RH / (2^16 - 1)) */
    uint16_t raw_humi = ((uint16_t)buf[3] << 8) | buf[4];
    *humi = 100.0f * ((float)raw_humi / 65535.0f);

    /* TODO: 根据CRC(buf[2], buf[5])验证数据完整性 */

    return ERR_OK;
}

/**
 * @brief   读取 BH1750 光照传感器
 * @note    I2C地址 0x23，连续高分辨率模式
 */
static int read_light(float *lux)
{
    uint8_t buf[2];
    int ret;

    /* 连续高分辨率模式指令 */
    uint8_t cmd = 0x10;
    ret = i2c_write_bytes(I2C_ADDR_LIGHT, 0x00, &cmd, 1);
    if (ret != ERR_OK) return ret;

    os_delay_ms(180);  /* 高分辨率模式测量时间 ~180ms */

    ret = i2c_read_bytes(I2C_ADDR_LIGHT, 0x00, buf, 2);
    if (ret != ERR_OK) return ret;

    /* 解析光照: Lux = (High_Byte << 8 | Low_Byte) / 1.2 */
    uint16_t raw_lux = ((uint16_t)buf[0] << 8) | buf[1];
    *lux = (float)raw_lux / 1.2f;

    return ERR_OK;
}

/**
 * @brief   读取 SCD30 CO2传感器
 * @note    I2C地址 0x61
 */
static int read_co2(float *co2)
{
    uint8_t buf[18];
    int ret;

    /* 读取测量数据 (命令 0x0300 或直接读) */
    uint8_t cmd[] = {0x03, 0x00};
    ret = i2c_write_bytes(I2C_ADDR_CO2, cmd[0], &cmd[1], 1);
    if (ret != ERR_OK) return ret;

    os_delay_ms(5);

    ret = i2c_read_bytes(I2C_ADDR_CO2, 0x00, buf, 18);
    if (ret != ERR_OK) return ret;

    /* CO2 = (buf[0]<<24 | buf[1]<<16 | buf[3]<<8 | buf[4]) 浮点数 */
    uint32_t raw_co2 = ((uint32_t)buf[0] << 24)
                     | ((uint32_t)buf[1] << 16)
                     | ((uint32_t)buf[3] << 8)
                     | buf[4];
    *co2 = *(float *)&raw_co2;

    return ERR_OK;
}

/**
 * @brief   读取土壤湿度传感器 (SPI接口)
 */
static int read_soil_moisture(float *moisture)
{
    uint8_t buf[2];
    int ret;

    ret = spi_read_bytes(SPI_CS_SOIL_MOISTURE, buf, 2);
    if (ret != ERR_OK) return ret;

    /* 12位ADC值转换为百分比 */
    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    *moisture = (float)(raw & 0x0FFF) / 40.95f;  /* 0-4095 -> 0-100% */
    *moisture = 100.0f - *moisture;  /* 干燥时电压高，需要反转 */

    if (*moisture < 0.0f) *moisture = 0.0f;
    if (*moisture > 100.0f) *moisture = 100.0f;

    return ERR_OK;
}

/* ================================================================
 * 综合采集任务
 * ================================================================ */

/**
 * @brief   执行一次完整的环境数据采集
 * @return  ERR_OK 或错误码
 */
static int sensor_collect_all(env_data_t *env)
{
    int ret;
    float temp, humi, lux, co2, soil;

    /* 通过信号量获取数据修改权 */
    os_sem_t *sem = get_sem_sensor_data();
    ret = os_sem_acquire(sem, 100);
    if (ret != ERR_OK) return ret;

    /* ---- 读取温湿度 ---- */
    ret = read_temp_humi(&temp, &humi);
    if (ret == ERR_OK) {
        env->temperature = data_filter_sliding_window(temp, SENSOR_TEMP_HUMI);
        env->humidity = data_filter_sliding_window(humi, SENSOR_TEMP_HUMI);
        g_sensor_online_mask |= (1 << SENSOR_TEMP_HUMI);
    } else {
        g_sensor_online_mask &= ~(1 << SENSOR_TEMP_HUMI);
    }

    /* ---- 读取光照 ---- */
    ret = read_light(&lux);
    if (ret == ERR_OK) {
        env->light_intensity = data_filter_sliding_window(lux, SENSOR_LIGHT);
        g_sensor_online_mask |= (1 << SENSOR_LIGHT);
    } else {
        g_sensor_online_mask &= ~(1 << SENSOR_LIGHT);
    }

    /* ---- 读取CO2 ---- */
    ret = read_co2(&co2);
    if (ret == ERR_OK) {
        env->co2_concentration = data_filter_sliding_window(co2, SENSOR_CO2);
        g_sensor_online_mask |= (1 << SENSOR_CO2);
    } else {
        g_sensor_online_mask &= ~(1 << SENSOR_CO2);
    }

    /* ---- 读取土壤湿度 ---- */
    ret = read_soil_moisture(&soil);
    if (ret == ERR_OK) {
        env->soil_moisture = data_filter_sliding_window(soil, SENSOR_SOIL_MOIST);
        g_sensor_online_mask |= (1 << SENSOR_SOIL_MOIST);
    } else {
        g_sensor_online_mask &= ~(1 << SENSOR_SOIL_MOIST);
    }

    env->timestamp = os_get_tick();
    env->sensor_status = g_sensor_online_mask;

    os_sem_release(sem);
    return ERR_OK;
}

/**
 * @brief   传感器采集任务入口
 * @note    周期性执行，采集数据后发送到消息队列
 */
void sensor_task_entry(void *arg)
{
    (void)arg;
    message_t msg;
    uint32_t last_temp_time = 0;
    uint32_t last_soil_time = 0;
    uint32_t last_light_time = 0;
    uint32_t last_co2_time = 0;

    while (1) {
        uint32_t now = os_get_tick();

        /* 按不同周期调度不同传感器 */
        if ((now - last_temp_time) >= SAMPLE_INTERVAL_TEMP_HUMI) {
            float temp, humi;
            if (read_temp_humi(&temp, &humi) == ERR_OK) {
                os_sem_t *sem = get_sem_sensor_data();
                os_sem_acquire(sem, 100);
                g_env_data.temperature = data_filter_sliding_window(temp, 0);
                g_env_data.humidity = data_filter_sliding_window(humi, 0);
                g_env_data.timestamp = now;
                os_sem_release(sem);
            }
            last_temp_time = now;
        }

        if ((now - last_light_time) >= SAMPLE_INTERVAL_LIGHT) {
            float lux;
            if (read_light(&lux) == ERR_OK) {
                os_sem_t *sem = get_sem_sensor_data();
                os_sem_acquire(sem, 100);
                g_env_data.light_intensity = data_filter_sliding_window(lux, 1);
                os_sem_release(sem);
            }
            last_light_time = now;
        }

        if ((now - last_co2_time) >= SAMPLE_INTERVAL_CO2) {
            float co2;
            if (read_co2(&co2) == ERR_OK) {
                os_sem_t *sem = get_sem_sensor_data();
                os_sem_acquire(sem, 100);
                g_env_data.co2_concentration = data_filter_sliding_window(co2, 2);
                os_sem_release(sem);
            }
            last_co2_time = now;
        }

        if ((now - last_soil_time) >= SAMPLE_INTERVAL_SOIL) {
            float soil;
            if (read_soil_moisture(&soil) == ERR_OK) {
                os_sem_t *sem = get_sem_sensor_data();
                os_sem_acquire(sem, 100);
                g_env_data.soil_moisture = data_filter_sliding_window(soil, 3);
                os_sem_release(sem);
            }
            last_soil_time = now;
        }

        /* 发送数据到消息队列 */
        msg.type = MSG_SENSOR_DATA;
        msg.length = sizeof(env_data_t);
        msg.data = &g_env_data;
        msg.timestamp = now;
        os_queue_send(get_queue_sensor(), &msg, 50);

        os_delay_ms(200);  /* 200ms 调度间隔 */
    }
}

/**
 * @brief   获取最新的环境数据 (线程安全)
 */
int get_env_data(env_data_t *env)
{
    if (env == NULL) return ERR_INVALID_PARAM;

    os_sem_t *sem = get_sem_sensor_data();
    int ret = os_sem_acquire(sem, 100);
    if (ret != ERR_OK) return ret;

    *env = g_env_data;

    os_sem_release(sem);
    return ERR_OK;
}
