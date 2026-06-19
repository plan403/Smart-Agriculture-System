/**
 * @file    device_protocol.h
 * @brief   设备通信协议定义
 * @note    定义云端与设备端通信协议帧格式
 */

#ifndef __DEVICE_PROTOCOL_H__
#define __DEVICE_PROTOCOL_H__

#include "common_types.h"

/* ================================================================
 * 协议帧常量
 * ================================================================ */
#define PROTOCOL_FRAME_HEADER       0xAA    /* 帧头 */
#define PROTOCOL_FRAME_FOOTER       0x55    /* 帧尾 */
#define PROTOCOL_MAX_PAYLOAD_SIZE   256     /* 最大载荷 */
#define PROTOCOL_DEV_ID_LEN         8       /* 设备ID长度 */

/* ================================================================
 * 云端下发指令类型
 * ================================================================ */
typedef enum {
    CLOUD_CMD_HEARTBEAT     = 0x00,         /* 心跳 */
    CLOUD_CMD_DEVICE_CTRL   = 0x01,         /* 设备控制 */
    CLOUD_CMD_THRESHOLD_SET = 0x02,         /* 阈值设置 */
    CLOUD_CMD_DATA_QUERY    = 0x03,         /* 数据查询 */
    CLOUD_CMD_OTA_START     = 0x04,         /* OTA升级开始 */
    CLOUD_CMD_SYS_REBOOT    = 0x05,         /* 系统重启 */
    CLOUD_CMD_SENSOR_SCAN   = 0x06          /* 传感器扫描 */
} cloud_cmd_type_t;

/* ================================================================
 * 云端下发指令帧格式
 * ================================================================ */
#pragma pack(1)
typedef struct {
    uint8_t     header;                     /* 帧头 0xAA */
    cloud_cmd_type_t cmd_type;              /* 指令类型 */
    uint8_t     payload_len;                /* 载荷长度 */
    uint8_t     payload[PROTOCOL_MAX_PAYLOAD_SIZE]; /* 载荷数据 */
    uint16_t    crc16;                      /* CRC16校验 */
    uint8_t     footer;                     /* 帧尾 0x55 */
} cloud_cmd_frame_t;
#pragma pack()

/* ================================================================
 * 设备上行数据帧格式
 * ================================================================ */
#pragma pack(1)
typedef struct {
    uint8_t     header;                     /* 帧头 0xAA */
    uint8_t     dev_id[PROTOCOL_DEV_ID_LEN]; /* 设备ID */
    uint8_t     msg_type;                   /* 消息类型 */
    uint8_t     payload_len;                /* 载荷长度 */
    uint8_t     payload[PROTOCOL_MAX_PAYLOAD_SIZE]; /* 载荷数据 */
    uint16_t    crc16;                      /* CRC16校验 */
    uint8_t     footer;                     /* 帧尾 0x55 */
} device_uplink_frame_t;
#pragma pack()

/* ================================================================
 * 标准化设备接入接口描述
 * ================================================================ */
typedef struct {
    char        device_name[32];            /* 设备名称 */
    uint8_t     device_type;                /* 设备类型 */
    uint8_t     comm_type;                  /* 通信类型: I2C/SPI/UART */
    uint8_t     address;                    /* 设备地址 */
    int (*init)(void);                      /* 初始化函数指针 */
    int (*read)(void *data, uint16_t len);  /* 读取函数指针 */
    int (*write)(void *data, uint16_t len); /* 写入函数指针 */
    int (*deinit)(void);                    /* 反初始化函数指针 */
} device_interface_t;

/* ================================================================
 * API 函数声明
 * ================================================================ */
int  protocol_parse_frame(uint8_t *buf, uint16_t len, cloud_cmd_frame_t *frame);
int  protocol_build_uplink(uint8_t *buf, uint16_t *len, device_uplink_frame_t *frame);
int  protocol_register_device(device_interface_t *dev_if);
int  protocol_unregister_device(uint8_t device_type);
void protocol_dump_frame(uint8_t *buf, uint16_t len);

#endif /* __DEVICE_PROTOCOL_H__ */
