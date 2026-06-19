/**
 * @file    cloud_proto.c
 * @brief   云端通信协议处理
 * @note    处理与云端的双向数据通信
 *          支持 JSON/MQTT 协议转换 (透传模式)
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"
#include "device_protocol.h"

/* ================================================================
 * 协议帧构建
 * ================================================================ */

/**
 * @brief   构建设备上行数据帧
 * @param   buf:    输出缓冲区
 * @param   len:    输出实际长度
 * @param   frame:  上行帧数据
 * @return  ERR_OK 或错误码
 */
int protocol_build_uplink(uint8_t *buf, uint16_t *len,
                           device_uplink_frame_t *frame)
{
    uint16_t offset = 0;
    extern uint16_t crc16_calculate(uint8_t *data, uint16_t len);

    if (buf == NULL || len == NULL || frame == NULL) {
        return ERR_INVALID_PARAM;
    }

    /* 帧头 */
    buf[offset++] = PROTOCOL_FRAME_HEADER;

    /* 设备ID */
    for (int i = 0; i < PROTOCOL_DEV_ID_LEN; i++) {
        buf[offset++] = frame->dev_id[i];
    }

    /* 消息类型 */
    buf[offset++] = frame->msg_type;

    /* 载荷长度 */
    buf[offset++] = frame->payload_len;

    /* 载荷数据 */
    for (int i = 0; i < frame->payload_len; i++) {
        buf[offset++] = frame->payload[i];
    }

    /* CRC16 (先占位) */
    uint16_t crc_offset = offset;
    buf[offset++] = 0;
    buf[offset++] = 0;

    /* 帧尾 */
    buf[offset++] = PROTOCOL_FRAME_FOOTER;

    /* 计算并填充CRC */
    uint16_t crc = crc16_calculate(buf, crc_offset);
    buf[crc_offset]     = (uint8_t)(crc >> 8);
    buf[crc_offset + 1] = (uint8_t)(crc & 0xFF);

    *len = offset;
    return ERR_OK;
}

/**
 * @brief   打印协议帧 (调试用)
 */
void protocol_dump_frame(uint8_t *buf, uint16_t len)
{
    extern int uart_send_string(uint8_t uart, const char *str);
    char hex_buf[8];

    uart_send_string(1, "[PROTO] ");
    for (uint16_t i = 0; i < len && i < 64; i++) {
        /* 转十六进制字符串 */
        hex_buf[0] = "0123456789ABCDEF"[buf[i] >> 4];
        hex_buf[1] = "0123456789ABCDEF"[buf[i] & 0x0F];
        hex_buf[2] = ' ';
        hex_buf[3] = '\0';
        uart_send_string(1, hex_buf);
    }
    uart_send_string(1, "\r\n");
}

/**
 * @brief   云端连接状态机
 */
typedef enum {
    CLOUD_DISCONNECTED = 0,
    CLOUD_CONNECTING,
    CLOUD_CONNECTED,
    CLOUD_AUTHENTICATING,
    CLOUD_ONLINE,
    CLOUD_ERROR
} cloud_state_t;

static cloud_state_t g_cloud_state = CLOUD_DISCONNECTED;

/**
 * @brief   AT指令发送WiFi连接 (ESP32透传模式)
 */
int cloud_wifi_connect(const char *ssid, const char *password)
{
    extern int uart_send_string(uint8_t uart, const char *str);
    char at_cmd[128];

    g_cloud_state = CLOUD_CONNECTING;

    /* AT+CWJAP="ssid","password" */
    int idx = 0;
    at_cmd[idx++] = 'A'; at_cmd[idx++] = 'T';
    at_cmd[idx++] = '+'; at_cmd[idx++] = 'C';
    at_cmd[idx++] = 'W'; at_cmd[idx++] = 'J';
    at_cmd[idx++] = 'A'; at_cmd[idx++] = 'P';
    at_cmd[idx++] = '='; at_cmd[idx++] = '"';
    while (*ssid) at_cmd[idx++] = *ssid++;
    at_cmd[idx++] = '"'; at_cmd[idx++] = ',';
    at_cmd[idx++] = '"';
    while (*password) at_cmd[idx++] = *password++;
    at_cmd[idx++] = '"';
    at_cmd[idx++] = '\r'; at_cmd[idx++] = '\n';
    at_cmd[idx] = '\0';

    uart_send_string(2, at_cmd);
    g_cloud_state = CLOUD_CONNECTED;

    return ERR_OK;
}

/**
 * @brief   获取云端连接状态
 */
cloud_state_t cloud_get_state(void)
{
    return g_cloud_state;
}

/**
 * @brief   设置云端连接状态
 */
void cloud_set_state(cloud_state_t state)
{
    g_cloud_state = state;
}
