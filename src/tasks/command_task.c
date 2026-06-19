/**
 * @file    command_task.c
 * @brief   指令解析任务 - 解析云端下发指令与本地指令
 * @note    最高优先级任务，确保指令实时响应
 *          支持设备控制、阈值设置、数据查询、OTA等指令
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"
#include "device_protocol.h"

/* 外部接口 */
extern int uart_recv_bytes(uint8_t uart, uint8_t *data, uint16_t len, uint32_t timeout_ms);
extern int uart_send_bytes(uint8_t uart, uint8_t *data, uint16_t len);
extern os_queue_t* get_queue_command(void);
extern os_queue_t* get_queue_report(void);
extern os_sem_t* get_sem_uart_tx(void);
extern void os_delay_ms(uint32_t ms);
extern uint32_t os_get_tick(void);
extern uint16_t crc16_calculate(uint8_t *data, uint16_t len);

/* 外部可修改的阈值变量 (声明) */
extern float g_threshold_temp_high;
extern float g_threshold_temp_low;
extern float g_threshold_humi_high;
extern float g_threshold_humi_low;
extern float g_threshold_soil_dry;
extern float g_threshold_light_high;
extern float g_threshold_light_low;
extern float g_threshold_co2_high;
extern float g_threshold_co2_low;

/* ================================================================
 * 协议帧解析
 * ================================================================ */

/**
 * @brief   解析云端下发指令帧
 * @param   buf:   接收缓冲区
 * @param   len:   数据长度
 * @param   frame: 输出解析后的帧
 * @return  ERR_OK 或错误码
 */
int protocol_parse_frame(uint8_t *buf, uint16_t len, cloud_cmd_frame_t *frame)
{
    if (buf == NULL || frame == NULL || len < 6) {
        return ERR_INVALID_PARAM;
    }

    /* 检查帧头尾 */
    if (buf[0] != PROTOCOL_FRAME_HEADER) return ERR_PROTOCOL_PARSE;
    if (buf[len - 1] != PROTOCOL_FRAME_FOOTER) return ERR_PROTOCOL_PARSE;

    /* 解析帧 */
    frame->header = buf[0];
    frame->cmd_type = (cloud_cmd_type_t)buf[1];
    frame->payload_len = buf[2];

    if (frame->payload_len > PROTOCOL_MAX_PAYLOAD_SIZE) {
        return ERR_PROTOCOL_PARSE;
    }

    /* 拷贝载荷 */
    for (uint8_t i = 0; i < frame->payload_len; i++) {
        frame->payload[i] = buf[3 + i];
    }

    /* 提取CRC */
    uint16_t crc_offset = 3 + frame->payload_len;
    frame->crc16 = ((uint16_t)buf[crc_offset] << 8) | buf[crc_offset + 1];
    frame->footer = buf[crc_offset + 2];

    /* 验证CRC */
    uint16_t calc_crc = crc16_calculate(buf, crc_offset);
    if (calc_crc != frame->crc16) {
        return ERR_CRC_MISMATCH;
    }

    return ERR_OK;
}

/* ================================================================
 * 指令处理器
 * ================================================================ */

/**
 * @brief   处理设备控制指令
 */
static int cmd_handle_device_ctrl(cloud_cmd_frame_t *frame)
{
    device_cmd_t dev_cmd;

    if (frame->payload_len < 2) return ERR_INVALID_PARAM;

    dev_cmd.device_type = (actuator_type_t)frame->payload[0];
    dev_cmd.command = frame->payload[1];
    dev_cmd.is_manual = 0;  /* 云端控制 */
    dev_cmd.timestamp = os_get_tick();

    /* 发送到设备控制队列 */
    return os_queue_send(get_queue_command(), &dev_cmd, 50);
}

/**
 * @brief   处理阈值设置指令
 */
static int cmd_handle_threshold_set(cloud_cmd_frame_t *frame)
{
    /* 载荷格式: [类型(1B)][值(4B float)] */
    if (frame->payload_len < 5) return ERR_INVALID_PARAM;

    uint8_t alarm_type = frame->payload[0];
    float *value = (float *)&frame->payload[1];

    switch (alarm_type) {
        case ALARM_TYPE_TEMP_HIGH:  g_threshold_temp_high = *value; break;
        case ALARM_TYPE_TEMP_LOW:   g_threshold_temp_low  = *value; break;
        case ALARM_TYPE_HUMI_HIGH:  g_threshold_humi_high = *value; break;
        case ALARM_TYPE_HUMI_LOW:   g_threshold_humi_low  = *value; break;
        case ALARM_TYPE_SOIL_DRY:   g_threshold_soil_dry  = *value; break;
        case ALARM_TYPE_LIGHT_HIGH: g_threshold_light_high = *value; break;
        case ALARM_TYPE_LIGHT_LOW:  g_threshold_light_low  = *value; break;
        case ALARM_TYPE_CO2_HIGH:   g_threshold_co2_high   = *value; break;
        case ALARM_TYPE_CO2_LOW:    g_threshold_co2_low    = *value; break;
        default: return ERR_INVALID_PARAM;
    }
    return ERR_OK;
}

/**
 * @brief   处理数据查询指令 (立即上报)
 */
static int cmd_handle_data_query(cloud_cmd_frame_t *frame)
{
    (void)frame;
    /* 触发立即上报 (发送高优先级上报消息) */
    /* 实现: 向上报队列发送紧急上报请求 */
    message_t msg;
    msg.type = MSG_CLOUD_CMD;
    msg.length = 0;
    msg.data = NULL;
    msg.timestamp = os_get_tick();
    return os_queue_send(get_queue_report(), &msg, 50);
}

/**
 * @brief   处理系统重启指令
 */
static int cmd_handle_sys_reboot(cloud_cmd_frame_t *frame)
{
    (void)frame;

    /* 发送重启确认 */
    uint8_t ack = 0x06;
    uart_send_bytes(2, &ack, 1);
    os_delay_ms(100);

    /* 触发系统复位 */
    __disable_irq();
    /* 设置 SCB AIRCR 系统复位 */
    *((volatile uint32_t *)0xE000ED0C) = 0x05FA0004;
    while (1) { __asm__ volatile ("nop"); }
    return ERR_OK;  /* 不会执行到 */
}

/* ================================================================
 * 指令解析任务入口
 * ================================================================ */

void command_task_entry(void *arg)
{
    (void)arg;
    uint8_t buf[PROTOCOL_MAX_PAYLOAD_SIZE + 10];  /* 接收缓冲 */
    cloud_cmd_frame_t frame;
    int ret;

    while (1) {
        /* 尝试从UART2接收指令 (非阻塞) */
        ret = uart_recv_bytes(2, buf, 1, 1);
        if (ret != ERR_OK) {
            os_delay_ms(50);
            continue;
        }

        /* 检测帧头 */
        if (buf[0] != PROTOCOL_FRAME_HEADER) {
            continue;
        }

        /* 读取帧头后的长度信息 */
        uint8_t header_buf[3];
        header_buf[0] = buf[0];
        ret = uart_recv_bytes(2, &header_buf[1], 2, 500);
        if (ret != ERR_OK) continue;

        uint8_t cmd_type = header_buf[1];
        uint8_t payload_len = header_buf[2];
        uint16_t total_len = 3 + payload_len + 2 + 1;  /* 头(1)+类型(1)+长度(1)+载荷+CRC(2)+尾(1) */

        if (total_len > sizeof(buf)) continue;

        /* 读取剩余数据 */
        buf[0] = header_buf[0];
        buf[1] = header_buf[1];
        buf[2] = header_buf[2];
        ret = uart_recv_bytes(2, &buf[3], total_len - 3, 1000);
        if (ret != ERR_OK) continue;

        /* 解析帧 */
        ret = protocol_parse_frame(buf, total_len, &frame);
        if (ret != ERR_OK) {
            /* 发送NAK */
            uint8_t nak = 0x15;
            uart_send_bytes(2, &nak, 1);
            continue;
        }

        /* 分发处理 */
        switch (frame.cmd_type) {
            case CLOUD_CMD_DEVICE_CTRL:
                ret = cmd_handle_device_ctrl(&frame);
                break;
            case CLOUD_CMD_THRESHOLD_SET:
                ret = cmd_handle_threshold_set(&frame);
                break;
            case CLOUD_CMD_DATA_QUERY:
                ret = cmd_handle_data_query(&frame);
                break;
            case CLOUD_CMD_OTA_START:
                /* TODO: OTA升级处理 */
                ret = ERR_OK;
                break;
            case CLOUD_CMD_SYS_REBOOT:
                ret = cmd_handle_sys_reboot(&frame);
                break;
            case CLOUD_CMD_HEARTBEAT:
            default:
                ret = ERR_OK;
                break;
        }

        /* 发送响应 */
        uint8_t response = (ret == ERR_OK) ? 0x06 : 0x15;
        os_sem_t *sem = get_sem_uart_tx();
        os_sem_acquire(sem, 100);
        uart_send_bytes(2, &response, 1);
        os_sem_release(sem);

        os_delay_ms(10);
    }
}
