/**
 * @file    main.c
 * @brief   SmartAgricultureOS 主入口
 * @note    基于 OpenHarmony LiteOS 的智慧农业嵌入式控制系统
 *          完成系统初始化、任务创建、调度器启动
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"
#include "pin_config.h"
#include "device_protocol.h"

/* ---- 驱动层 ---- */
extern int gpio_init_peripherals(void);
extern int i2c_init_all(void);
extern int spi_init_all(void);
extern int uart_init_all(void);

/* ---- 任务管理器 ---- */
extern int task_manager_init(void);
extern uint8_t os_task_create(void (*entry)(void *), void *arg,
                               uint8_t priority, uint32_t stack_size);
extern void os_scheduler_start(void);
extern void os_delay_ms(uint32_t ms);

/* ---- 任务入口 ---- */
extern void sensor_task_entry(void *arg);
extern void control_task_entry(void *arg);
extern void alarm_task_entry(void *arg);
extern void report_task_entry(void *arg);
extern void command_task_entry(void *arg);

/* ---- 业务层 ---- */
extern int actuator_init_all(void);
extern int threshold_load_from_eeprom(void);

/* ---- 工具 ---- */
extern void data_filter_reset_all(void);
extern uint32_t os_get_tick(void);

/* ================================================================
 * 系统状态
 * ================================================================ */
static sys_state_t g_sys_state = SYS_STATE_INIT;

/**
 * @brief   获取系统状态
 */
sys_state_t sys_get_state(void)
{
    return g_sys_state;
}

/**
 * @brief   设置系统状态
 */
void sys_set_state(sys_state_t state)
{
    g_sys_state = state;
}

/* ================================================================
 * 系统初始化
 * ================================================================ */

/**
 * @brief   硬件层初始化
 * @note    初始化顺序很重要: GPIO -> I2C -> SPI -> UART
 */
static int hal_init(void)
{
    int ret;

    /* 1. GPIO 初始化 (蜂鸣器/LED/按键/执行设备) */
    ret = gpio_init_peripherals();
    if (ret != ERR_OK) {
        return ret;
    }

    /* 2. I2C 总线初始化 (温湿度/光照/CO2传感器) */
    ret = i2c_init_all();
    if (ret != ERR_OK) {
        return ret;
    }

    /* 3. SPI 总线初始化 (土壤湿度传感器) */
    ret = spi_init_all();
    if (ret != ERR_OK) {
        return ret;
    }

    /* 4. UART 初始化 (调试串口 + 云端通信) */
    ret = uart_init_all();
    if (ret != ERR_OK) {
        return ret;
    }

    /* 5. 执行设备初始化为关闭状态 */
    ret = actuator_init_all();
    if (ret != ERR_OK) {
        return ret;
    }

    return ERR_OK;
}

/**
 * @brief   软件层初始化
 */
static int sw_init(void)
{
    int ret;

    /* 1. 初始化任务管理器 (信号量/消息队列) */
    ret = task_manager_init();
    if (ret != ERR_OK) {
        return ret;
    }

    /* 2. 初始化数据滤波器 */
    data_filter_reset_all();

    /* 3. 从EEPROM加载阈值配置 */
    threshold_load_from_eeprom();

    return ERR_OK;
}

/**
 * @brief   创建所有系统任务
 */
static int tasks_create(void)
{
    uint8_t task_id;

    /* 最高优先级: 指令解析 (确保实时响应) */
    task_id = os_task_create(command_task_entry, NULL,
                              PRIO_CMD_PARSE, STACK_COMMAND);
    if (task_id == 0xFF) return ERR_TASK_CREATE;

    /* 高优先级: 异常检测 */
    task_id = os_task_create(alarm_task_entry, NULL,
                              PRIO_ALARM_DETECT, STACK_ALARM);
    if (task_id == 0xFF) return ERR_TASK_CREATE;

    /* 中优先级: 设备控制 */
    task_id = os_task_create(control_task_entry, NULL,
                              PRIO_DEVICE_CTRL, STACK_CONTROL);
    if (task_id == 0xFF) return ERR_TASK_CREATE;

    /* 中优先级: 数据上报 */
    task_id = os_task_create(report_task_entry, NULL,
                              PRIO_DATA_REPORT, STACK_REPORT);
    if (task_id == 0xFF) return ERR_TASK_CREATE;

    /* 低优先级: 环境采集 */
    task_id = os_task_create(sensor_task_entry, NULL,
                              PRIO_SENSOR_COLLECT, STACK_SENSOR);
    if (task_id == 0xFF) return ERR_TASK_CREATE;

    return ERR_OK;
}

/* ================================================================
 * 主函数
 * ================================================================ */

int main(void)
{
    int ret;

    /* ---- 第一阶段: 硬件初始化 ---- */
    g_sys_state = SYS_STATE_INIT;

    ret = hal_init();
    if (ret != ERR_OK) {
        /* 硬件初始化失败 - 快速闪烁LED报警 */
        while (1) {
            /* LED 快速闪烁指示故障 */
            __asm__ volatile ("nop");
        }
    }

    /* ---- 第二阶段: 软件初始化 ---- */
    ret = sw_init();
    if (ret != ERR_OK) {
        while (1) { __asm__ volatile ("nop"); }
    }

    /* ---- 第三阶段: 创建任务 ---- */
    ret = tasks_create();
    if (ret != ERR_OK) {
        while (1) { __asm__ volatile ("nop"); }
    }

    /* ---- 第四阶段: 系统就绪 ---- */
    g_sys_state = SYS_STATE_RUNNING;

    /* 启动LED指示系统就绪 */
    /* led_set(1); */

    /* ---- 第五阶段: 启动调度器 (此函数不返回) ---- */
    os_scheduler_start();

    /* 永远不会执行到这里 */
    return 0;
}

/* ================================================================
 * 系统异常处理
 * ================================================================ */

/**
 * @brief   硬件错误处理 (HardFault)
 */
void HardFault_Handler(void)
{
    g_sys_state = SYS_STATE_FAULT;
    while (1) {
        /* 蜂鸣器持续报警 */
        __asm__ volatile ("nop");
    }
}

/**
 * @brief   内存管理错误
 */
void MemManage_Handler(void)
{
    g_sys_state = SYS_STATE_FAULT;
    while (1) { __asm__ volatile ("nop"); }
}

/**
 * @brief   总线错误
 */
void BusFault_Handler(void)
{
    g_sys_state = SYS_STATE_FAULT;
    while (1) { __asm__ volatile ("nop"); }
}

/**
 * @brief   看门狗中断
 */
void WDT_IRQHandler(void)
{
    /* 喂狗操作，记录日志 */
    g_sys_state = SYS_STATE_FAULT;
    while (1) { __asm__ volatile ("nop"); }
}
