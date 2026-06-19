/**
 * @file    threshold_mgr.c
 * @brief   阈值管理器
 * @note    支持动态修改阈值，持久化存储(EEPROM/Flash)
 *          多级阈值: 正常 -> 警告 -> 临界
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"

/* ================================================================
 * 全局阈值变量 (可运行时动态修改)
 * ================================================================ */
float g_threshold_temp_high     = TEMP_HIGH_THRESHOLD;
float g_threshold_temp_low      = TEMP_LOW_THRESHOLD;
float g_threshold_humi_high     = HUMI_HIGH_THRESHOLD;
float g_threshold_humi_low      = HUMI_LOW_THRESHOLD;
float g_threshold_soil_dry      = SOIL_DRY_THRESHOLD;
float g_threshold_light_high    = LIGHT_HIGH_THRESHOLD;
float g_threshold_light_low     = LIGHT_LOW_THRESHOLD;
float g_threshold_co2_high      = CO2_HIGH_THRESHOLD;
float g_threshold_co2_low       = CO2_LOW_THRESHOLD;

/* 滞后带 (回差)，防止在阈值附近频繁触发 */
static float g_hysteresis = 2.0f;

/* ================================================================
 * 阈值配置结构 (用于批量读写)
 * ================================================================ */
#pragma pack(1)
typedef struct {
    uint32_t magic;                     /* 魔数 0x54485244 ("THRD") */
    float temp_high, temp_low;
    float humi_high, humi_low;
    float soil_dry;
    float light_high, light_low;
    float co2_high, co2_low;
    float hysteresis;
    uint16_t crc16;
} threshold_config_t;
#pragma pack()

#define THRESHOLD_MAGIC     0x54485244
#define EEPROM_THRESHOLD_ADDR 0x0000   /* EEPROM存储地址 */

/* ================================================================
 * 阈值读写
 * ================================================================ */

/**
 * @brief   获取指定类型的阈值
 */
int threshold_get(alarm_type_t type, float *value)
{
    if (value == NULL) return ERR_INVALID_PARAM;

    switch (type) {
        case ALARM_TYPE_TEMP_HIGH:  *value = g_threshold_temp_high;  break;
        case ALARM_TYPE_TEMP_LOW:   *value = g_threshold_temp_low;   break;
        case ALARM_TYPE_HUMI_HIGH:  *value = g_threshold_humi_high;  break;
        case ALARM_TYPE_HUMI_LOW:   *value = g_threshold_humi_low;   break;
        case ALARM_TYPE_SOIL_DRY:   *value = g_threshold_soil_dry;   break;
        case ALARM_TYPE_LIGHT_HIGH: *value = g_threshold_light_high; break;
        case ALARM_TYPE_LIGHT_LOW:  *value = g_threshold_light_low;  break;
        case ALARM_TYPE_CO2_HIGH:   *value = g_threshold_co2_high;   break;
        case ALARM_TYPE_CO2_LOW:    *value = g_threshold_co2_low;    break;
        default: return ERR_INVALID_PARAM;
    }
    return ERR_OK;
}

/**
 * @brief   设置指定类型的阈值
 */
int threshold_set(alarm_type_t type, float value)
{
    switch (type) {
        case ALARM_TYPE_TEMP_HIGH:  g_threshold_temp_high  = value; break;
        case ALARM_TYPE_TEMP_LOW:   g_threshold_temp_low   = value; break;
        case ALARM_TYPE_HUMI_HIGH:  g_threshold_humi_high  = value; break;
        case ALARM_TYPE_HUMI_LOW:   g_threshold_humi_low   = value; break;
        case ALARM_TYPE_SOIL_DRY:   g_threshold_soil_dry   = value; break;
        case ALARM_TYPE_LIGHT_HIGH: g_threshold_light_high = value; break;
        case ALARM_TYPE_LIGHT_LOW:  g_threshold_light_low  = value; break;
        case ALARM_TYPE_CO2_HIGH:   g_threshold_co2_high   = value; break;
        case ALARM_TYPE_CO2_LOW:    g_threshold_co2_low    = value; break;
        default: return ERR_INVALID_PARAM;
    }
    return ERR_OK;
}

/**
 * @brief   获取滞后带
 */
float threshold_get_hysteresis(void)
{
    return g_hysteresis;
}

/**
 * @brief   设置滞后带
 */
void threshold_set_hysteresis(float value)
{
    if (value >= 0.0f && value <= 10.0f) {
        g_hysteresis = value;
    }
}

/**
 * @brief   恢复默认阈值
 */
void threshold_reset_default(void)
{
    g_threshold_temp_high  = TEMP_HIGH_THRESHOLD;
    g_threshold_temp_low   = TEMP_LOW_THRESHOLD;
    g_threshold_humi_high  = HUMI_HIGH_THRESHOLD;
    g_threshold_humi_low   = HUMI_LOW_THRESHOLD;
    g_threshold_soil_dry   = SOIL_DRY_THRESHOLD;
    g_threshold_light_high = LIGHT_HIGH_THRESHOLD;
    g_threshold_light_low  = LIGHT_LOW_THRESHOLD;
    g_threshold_co2_high   = CO2_HIGH_THRESHOLD;
    g_threshold_co2_low    = CO2_LOW_THRESHOLD;
    g_hysteresis = 2.0f;
}

/**
 * @brief   保存阈值到EEPROM (持久化)
 * @note    需要平台EEPROM驱动支持
 */
int threshold_save_to_eeprom(void)
{
    threshold_config_t cfg;
    extern uint16_t crc16_calculate(uint8_t *data, uint16_t len);

    cfg.magic       = THRESHOLD_MAGIC;
    cfg.temp_high   = g_threshold_temp_high;
    cfg.temp_low    = g_threshold_temp_low;
    cfg.humi_high   = g_threshold_humi_high;
    cfg.humi_low    = g_threshold_humi_low;
    cfg.soil_dry    = g_threshold_soil_dry;
    cfg.light_high  = g_threshold_light_high;
    cfg.light_low   = g_threshold_light_low;
    cfg.co2_high    = g_threshold_co2_high;
    cfg.co2_low     = g_threshold_co2_low;
    cfg.hysteresis  = g_hysteresis;

    /* 计算CRC */
    uint16_t data_len = (uint8_t *)&cfg.crc16 - (uint8_t *)&cfg;
    cfg.crc16 = crc16_calculate((uint8_t *)&cfg, data_len);

    /* TODO: 调用EEPROM写入函数 */
    /* eeprom_write(EEPROM_THRESHOLD_ADDR, (uint8_t *)&cfg, sizeof(cfg)); */

    return ERR_OK;
}

/**
 * @brief   从EEPROM加载阈值
 */
int threshold_load_from_eeprom(void)
{
    threshold_config_t cfg;
    extern uint16_t crc16_calculate(uint8_t *data, uint16_t len);

    /* TODO: 调用EEPROM读取函数 */
    /* eeprom_read(EEPROM_THRESHOLD_ADDR, (uint8_t *)&cfg, sizeof(cfg)); */

    /* 验证魔数 */
    if (cfg.magic != THRESHOLD_MAGIC) {
        /* 首次使用，使用默认值 */
        threshold_reset_default();
        return ERR_FAIL;
    }

    /* 验证CRC */
    uint16_t data_len = (uint8_t *)&cfg.crc16 - (uint8_t *)&cfg;
    uint16_t calc_crc = crc16_calculate((uint8_t *)&cfg, data_len);
    if (calc_crc != cfg.crc16) {
        threshold_reset_default();
        return ERR_CRC_MISMATCH;
    }

    /* 加载配置 */
    g_threshold_temp_high  = cfg.temp_high;
    g_threshold_temp_low   = cfg.temp_low;
    g_threshold_humi_high  = cfg.humi_high;
    g_threshold_humi_low   = cfg.humi_low;
    g_threshold_soil_dry   = cfg.soil_dry;
    g_threshold_light_high = cfg.light_high;
    g_threshold_light_low  = cfg.light_low;
    g_threshold_co2_high   = cfg.co2_high;
    g_threshold_co2_low    = cfg.co2_low;
    g_hysteresis           = cfg.hysteresis;

    return ERR_OK;
}
