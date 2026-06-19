/**
 * @file    system_config.h
 * @brief   系统参数配置文件
 * @note    统一管理系统所有可配置参数
 */

#ifndef __SYSTEM_CONFIG_H__
#define __SYSTEM_CONFIG_H__

/* ================================================================
 * 平台选择
 * ================================================================ */
#define PLATFORM_STM32      1
#define PLATFORM_ESP32      2

#ifndef TARGET_PLATFORM
#define TARGET_PLATFORM     PLATFORM_STM32
#endif

/* ================================================================
 * 系统时钟配置 (单位: Hz)
 * ================================================================ */
#define SYS_CORE_CLOCK      72000000    /* 72MHz 主频 */
#define APB1_CLOCK          36000000    /* APB1 总线时钟 */
#define APB2_CLOCK          72000000    /* APB2 总线时钟 */

/* ================================================================
 * LiteOS 内核配置
 * ================================================================ */
#define OS_TICK_RATE        1000        /* 系统滴答频率 1KHz */
#define OS_MAX_TASKS        16          /* 最大任务数 */
#define OS_MIN_STACK_SIZE   128         /* 最小任务栈 (字) */

/* ================================================================
 * 任务优先级配置 (数值越小优先级越高)
 * ================================================================ */
#define PRIO_CMD_PARSE      2           /* 指令解析 - 最高优先级 */
#define PRIO_ALARM_DETECT   3           /* 异常检测 */
#define PRIO_DEVICE_CTRL    4           /* 设备控制 */
#define PRIO_DATA_REPORT    5           /* 数据上报 */
#define PRIO_SENSOR_COLLECT 6           /* 环境采集 - 最低优先级 */

/* ================================================================
 * 任务栈大小配置 (单位: 字)
 * ================================================================ */
#define STACK_SENSOR        512
#define STACK_CONTROL       512
#define STACK_ALARM         384
#define STACK_REPORT        768
#define STACK_COMMAND       512

/* ================================================================
 * 传感器采集周期 (单位: ms)
 * ================================================================ */
#define SAMPLE_INTERVAL_TEMP_HUMI   2000    /* 温湿度采集周期 */
#define SAMPLE_INTERVAL_SOIL        5000    /* 土壤湿度采集周期 */
#define SAMPLE_INTERVAL_LIGHT       1000    /* 光照采集周期 */
#define SAMPLE_INTERVAL_CO2         3000    /* CO2浓度采集周期 */

/* ================================================================
 * 数据滤波配置
 * ================================================================ */
#define FILTER_WINDOW_SIZE      8           /* 滑动滤波窗口大小 */
#define FILTER_EXTREME_COUNT    2           /* 去极值剔除个数 */

/* ================================================================
 * 阈值预警默认配置
 * ================================================================ */
#define TEMP_HIGH_THRESHOLD     35.0f       /* 温度上限 (℃) */
#define TEMP_LOW_THRESHOLD      5.0f        /* 温度下限 (℃) */
#define HUMI_HIGH_THRESHOLD     90.0f       /* 湿度上限 (%) */
#define HUMI_LOW_THRESHOLD      20.0f       /* 湿度下限 (%) */
#define SOIL_DRY_THRESHOLD      15.0f       /* 土壤干旱阈值 (%) */
#define LIGHT_HIGH_THRESHOLD    80000.0f    /* 光照过强阈值 (Lux) */
#define LIGHT_LOW_THRESHOLD     5000.0f     /* 光照过弱阈值 (Lux) */
#define CO2_HIGH_THRESHOLD      2000.0f     /* CO2上限 (ppm) */
#define CO2_LOW_THRESHOLD       350.0f      /* CO2下限 (ppm) */

/* ================================================================
 * 通信配置
 * ================================================================ */
#define UART_BAUDRATE           115200      /* 串口波特率 */
#define I2C_SPEED               400000      /* I2C 速率 400KHz (Fast Mode) */
#define SPI_SPEED               10000000    /* SPI 速率 10MHz */
#define CLOUD_REPORT_INTERVAL   10000       /* 云端上报间隔 (ms) */

/* ================================================================
 * 看门狗配置
 * ================================================================ */
#define WDT_TIMEOUT_MS          5000        /* 看门狗超时 5s */

#endif /* __SYSTEM_CONFIG_H__ */
