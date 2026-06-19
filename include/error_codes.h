/**
 * @file    error_codes.h
 * @brief   系统错误码统一定义
 */

#ifndef __ERROR_CODES_H__
#define __ERROR_CODES_H__

/* ================================================================
 * 通用错误码
 * ================================================================ */
#define ERR_OK                      0x00    /* 操作成功 */
#define ERR_FAIL                    0x01    /* 操作失败 */
#define ERR_TIMEOUT                 0x02    /* 操作超时 */
#define ERR_BUSY                    0x03    /* 资源忙 */
#define ERR_INVALID_PARAM           0x04    /* 无效参数 */
#define ERR_NO_MEMORY               0x05    /* 内存不足 */
#define ERR_NOT_INITIALIZED         0x06    /* 未初始化 */
#define ERR_ALREADY_INITIALIZED     0x07    /* 已初始化 */

/* ================================================================
 * 外设驱动错误码 (0x10 - 0x2F)
 * ================================================================ */
#define ERR_GPIO_INIT               0x10    /* GPIO 初始化失败 */
#define ERR_GPIO_WRITE              0x11    /* GPIO 写入失败 */
#define ERR_GPIO_READ               0x12    /* GPIO 读取失败 */

#define ERR_I2C_INIT                0x13    /* I2C 初始化失败 */
#define ERR_I2C_TIMING              0x14    /* I2C 时序异常 */
#define ERR_I2C_NACK                0x15    /* I2C 从机无应答 */
#define ERR_I2C_ARBITRATION_LOST    0x16    /* I2C 仲裁丢失 */
#define ERR_I2C_BUS_ERROR           0x17    /* I2C 总线错误 */

#define ERR_SPI_INIT                0x18    /* SPI 初始化失败 */
#define ERR_SPI_TIMING              0x19    /* SPI 时序异常 */
#define ERR_SPI_OVERRUN             0x1A    /* SPI 数据溢出 */
#define ERR_SPI_PACKET_LOST         0x1B    /* SPI 数据丢包 */
#define ERR_SPI_CRC_FAIL            0x1C    /* SPI CRC校验失败 */

#define ERR_UART_INIT               0x1D    /* UART 初始化失败 */
#define ERR_UART_FRAME              0x1E    /* UART 帧错误 */
#define ERR_UART_PARITY             0x1F    /* UART 校验错误 */
#define ERR_UART_OVERRUN            0x20    /* UART 溢出错误 */
#define ERR_UART_NOISE              0x21    /* UART 噪声错误 */

/* ================================================================
 * 传感器错误码 (0x30 - 0x3F)
 * ================================================================ */
#define ERR_SENSOR_NOT_FOUND        0x30    /* 传感器未找到 */
#define ERR_SENSOR_READ_FAIL        0x31    /* 传感器读取失败 */
#define ERR_SENSOR_DATA_INVALID     0x32    /* 传感器数据无效 */
#define ERR_SENSOR_CALIBRATION      0x33    /* 传感器校准失败 */
#define ERR_SENSOR_DISCONNECTED     0x34    /* 传感器断连 */

/* ================================================================
 * 任务调度错误码 (0x40 - 0x4F)
 * ================================================================ */
#define ERR_TASK_CREATE             0x40    /* 任务创建失败 */
#define ERR_TASK_DELETE             0x41    /* 任务删除失败 */
#define ERR_TASK_SUSPEND            0x42    /* 任务挂起失败 */
#define ERR_TASK_RESUME             0x43    /* 任务恢复失败 */
#define ERR_SEM_CREATE              0x44    /* 信号量创建失败 */
#define ERR_SEM_TIMEOUT             0x45    /* 信号量获取超时 */
#define ERR_QUEUE_FULL              0x46    /* 消息队列满 */
#define ERR_QUEUE_EMPTY             0x47    /* 消息队列空 */
#define ERR_QUEUE_CREATE            0x48    /* 消息队列创建失败 */
#define ERR_STACK_OVERFLOW          0x49    /* 任务栈溢出 */

/* ================================================================
 * 通信错误码 (0x50 - 0x5F)
 * ================================================================ */
#define ERR_CLOUD_CONNECT           0x50    /* 云端连接失败 */
#define ERR_CLOUD_SEND              0x51    /* 云端发送失败 */
#define ERR_CLOUD_RECV              0x52    /* 云端接收失败 */
#define ERR_CLOUD_AUTH              0x53    /* 云端认证失败 */
#define ERR_PROTOCOL_PARSE          0x54    /* 协议解析错误 */
#define ERR_CRC_MISMATCH            0x55    /* CRC校验不匹配 */

/* ================================================================
 * 错误码转字符串宏
 * ================================================================ */
const char* error_to_string(uint8_t err_code);

#endif /* __ERROR_CODES_H__ */
