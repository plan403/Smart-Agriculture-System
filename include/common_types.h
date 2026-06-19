/**
 * @file    common_types.h
 * @brief   项目通用数据类型定义
 */

#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * 系统运行状态
 * ================================================================ */
typedef enum {
    SYS_STATE_INIT          = 0x00,     /* 系统初始化 */
    SYS_STATE_RUNNING       = 0x01,     /* 正常运行 */
    SYS_STATE_ALARM         = 0x02,     /* 预警状态 */
    SYS_STATE_FAULT         = 0x03,     /* 故障状态 */
    SYS_STATE_LOW_POWER     = 0x04      /* 低功耗模式 */
} sys_state_t;

/* ================================================================
 * 传感器类型枚举
 * ================================================================ */
typedef enum {
    SENSOR_TEMP_HUMI    = 0x01,         /* 温湿度传感器 */
    SENSOR_SOIL_MOIST   = 0x02,         /* 土壤湿度传感器 */
    SENSOR_LIGHT        = 0x03,         /* 光照传感器 */
    SENSOR_CO2          = 0x04          /* CO2传感器 */
} sensor_type_t;

/* ================================================================
 * 执行设备类型枚举
 * ================================================================ */
typedef enum {
    ACTUATOR_PUMP       = 0x01,         /* 水泵 */
    ACTUATOR_CURTAIN    = 0x02,         /* 遮阳帘 */
    ACTUATOR_FAN        = 0x03,         /* 通风风扇 */
    ACTUATOR_BUZZER     = 0x04,         /* 蜂鸣器 */
    ACTUATOR_MAX
} actuator_type_t;

/* ================================================================
 * 设备状态枚举
 * ================================================================ */
typedef enum {
    DEVICE_OFF          = 0x00,
    DEVICE_ON           = 0x01,
    DEVICE_FAULT        = 0x02,
    DEVICE_DISCONNECTED = 0x03
} device_state_t;

/* ================================================================
 * 预警级别枚举
 * ================================================================ */
typedef enum {
    ALARM_NONE      = 0,                /* 无预警 */
    ALARM_INFO      = 1,                /* 信息提示 */
    ALARM_WARNING   = 2,                /* 警告 */
    ALARM_CRITICAL  = 3                 /* 严重 */
} alarm_level_t;

/* ================================================================
 * 预警类型枚举
 * ================================================================ */
typedef enum {
    ALARM_TYPE_TEMP_HIGH    = 0x01,
    ALARM_TYPE_TEMP_LOW     = 0x02,
    ALARM_TYPE_HUMI_HIGH    = 0x03,
    ALARM_TYPE_HUMI_LOW     = 0x04,
    ALARM_TYPE_SOIL_DRY     = 0x05,
    ALARM_TYPE_LIGHT_HIGH   = 0x06,
    ALARM_TYPE_LIGHT_LOW    = 0x07,
    ALARM_TYPE_CO2_HIGH     = 0x08,
    ALARM_TYPE_CO2_LOW      = 0x09,
    ALARM_TYPE_DEVICE_OFFLINE = 0x0A
} alarm_type_t;

/* ================================================================
 * 环境数据结构体
 * ================================================================ */
#pragma pack(1)
typedef struct {
    float       temperature;            /* 温度 (℃) */
    float       humidity;               /* 湿度 (%) */
    float       soil_moisture;          /* 土壤湿度 (%) */
    float       light_intensity;        /* 光照强度 (Lux) */
    float       co2_concentration;      /* CO2浓度 (ppm) */
    uint32_t    timestamp;              /* 采集时间戳 */
    uint8_t     sensor_status;          /* 传感器状态位掩码 */
} env_data_t;
#pragma pack()

/* ================================================================
 * 设备控制指令结构体
 * ================================================================ */
#pragma pack(1)
typedef struct {
    actuator_type_t device_type;        /* 设备类型 */
    uint8_t         command;            /* 控制指令: 0=关, 1=开 */
    uint8_t         is_manual;          /* 是否手动控制 */
    uint32_t        timestamp;          /* 指令时间戳 */
} device_cmd_t;
#pragma pack()

/* ================================================================
 * 预警信息结构体
 * ================================================================ */
#pragma pack(1)
typedef struct {
    alarm_type_t    type;               /* 预警类型 */
    alarm_level_t   level;              /* 预警级别 */
    float           current_value;      /* 当前值 */
    float           threshold_value;    /* 触发阈值 */
    uint32_t        timestamp;          /* 触发时间戳 */
    char            message[64];        /* 预警描述信息 */
} alarm_info_t;
#pragma pack()

/* ================================================================
 * 云端数据上报结构体
 * ================================================================ */
#pragma pack(1)
typedef struct {
    uint8_t     header;                 /* 帧头 0xAA */
    uint8_t     dev_id[8];             /* 设备ID */
    env_data_t  env;                    /* 环境数据 */
    uint8_t     alarm_count;            /* 当前预警数量 */
    alarm_info_t alarms[4];             /* 预警详情(最多4条) */
    uint16_t    crc16;                  /* CRC16校验 */
    uint8_t     footer;                 /* 帧尾 0x55 */
} cloud_report_t;
#pragma pack()

/* ================================================================
 * 任务消息类型
 * ================================================================ */
typedef enum {
    MSG_SENSOR_DATA     = 0x01,         /* 传感器数据 */
    MSG_DEVICE_CMD      = 0x02,         /* 设备控制指令 */
    MSG_ALARM_INFO      = 0x03,         /* 预警信息 */
    MSG_CLOUD_CMD       = 0x04,         /* 云端下发指令 */
    MSG_SYS_EVENT       = 0x05          /* 系统事件 */
} msg_type_t;

/* ================================================================
 * 通用消息结构体
 * ================================================================ */
typedef struct {
    msg_type_t  type;                   /* 消息类型 */
    uint16_t    length;                 /* 数据长度 */
    void       *data;                   /* 数据指针 */
    uint32_t    timestamp;              /* 时间戳 */
} message_t;

#endif /* __COMMON_TYPES_H__ */
