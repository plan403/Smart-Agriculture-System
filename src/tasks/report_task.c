/**
 * @file    report_task.c
 * @brief   数据上报任务 - 定时向云端上报环境数据
 * @note    通过UART2与WiFi/4G模块通信
 *          组装协议帧并发送
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"
#include "device_protocol.h"

/* 外部接口 */
extern int get_env_data(env_data_t *env);
extern int uart_send_bytes(uint8_t uart, uint8_t *data, uint16_t len);
extern int uart_recv_bytes(uint8_t uart, uint8_t *data, uint16_t len, uint32_t timeout_ms);
extern os_sem_t* get_sem_uart_tx(void);
extern os_queue_t* get_queue_alarm(void);
extern os_queue_t* get_queue_report(void);
extern void os_delay_ms(uint32_t ms);
extern uint32_t os_get_tick(void);

/* 外部CRC计算 */
extern uint16_t crc16_calculate(uint8_t *data, uint16_t len);

/* ================================================================
 * 设备ID (生产时可从EEPROM读取)
 * ================================================================ */
static uint8_t g_dev_id[8] = {'S', 'A', 'O', 'S', '0', '0', '0', '1'};

/* ================================================================
 * 数据打包
 * ================================================================ */

/**
 * @brief   打包环境数据为上报帧
 */
static int build_report_frame(cloud_report_t *report)
{
    alarm_info_t alarm;
    int ret;

    /* 获取最新环境数据 */
    ret = get_env_data(&report->env);
    if (ret != ERR_OK) return ret;

    /* 填充帧头尾 */
    report->header = PROTOCOL_FRAME_HEADER;
    report->footer = PROTOCOL_FRAME_FOOTER;

    /* 拷贝设备ID */
    for (int i = 0; i < 8; i++) {
        report->dev_id[i] = g_dev_id[i];
    }

    /* 获取当前预警 (非阻塞) */
    report->alarm_count = 0;
    while (os_queue_recv(get_queue_alarm(), &alarm, 0) == ERR_OK) {
        if (report->alarm_count < 4) {
            report->alarms[report->alarm_count] = alarm;
            report->alarm_count++;
        }
    }

    /* 计算CRC (对除CRC字段外的所有数据) */
    uint16_t crc_offset = (uint8_t *)&report->crc16 - (uint8_t *)report;
    report->crc16 = crc16_calculate((uint8_t *)report, crc_offset);

    return ERR_OK;
}

/* ================================================================
 * 串口发送
 * ================================================================ */

/**
 * @brief   通过串口发送上报数据
 */
static int report_send(cloud_report_t *report)
{
    os_sem_t *sem = get_sem_uart_tx();
    int ret;

    /* 获取串口发送权 */
    ret = os_sem_acquire(sem, 200);
    if (ret != ERR_OK) return ret;

    /* 发送数据 */
    ret = uart_send_bytes(2, (uint8_t *)report, sizeof(cloud_report_t));

    os_sem_release(sem);
    return ret;
}

/* ================================================================
 * 云端响应处理
 * ================================================================ */

/**
 * @brief   等待云端ACK (简化实现)
 */
static int wait_cloud_ack(uint32_t timeout_ms)
{
    uint8_t ack_byte;
    int ret;

    ret = uart_recv_bytes(2, &ack_byte, 1, timeout_ms);
    if (ret != ERR_OK) return ERR_CLOUD_RECV;

    if (ack_byte == 0x06) {  /* ACK */
        return ERR_OK;
    } else if (ack_byte == 0x15) {  /* NAK */
        return ERR_CLOUD_SEND;
    }
    return ERR_PROTOCOL_PARSE;
}

/* ================================================================
 * 数据上报任务入口
 * ================================================================ */

void report_task_entry(void *arg)
{
    (void)arg;
    cloud_report_t report;
    uint32_t last_report = 0;

    while (1) {
        uint32_t now = os_get_tick();

        /* 按时上报 */
        if ((now - last_report) >= CLOUD_REPORT_INTERVAL) {
            int ret = build_report_frame(&report);
            if (ret == ERR_OK) {
                ret = report_send(&report);
                if (ret == ERR_OK) {
                    /* 等待云端确认 */
                    wait_cloud_ack(2000);
                }
            }
            last_report = now;
        }

        os_delay_ms(500);
    }
}
