/**
 * @file    actuator_mgr.c
 * @brief   执行设备管理器
 * @note    统一管理所有执行设备，支持手动/自动/远程三种控制模式
 *          内置设备故障检测与自动恢复机制
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"
#include "pin_config.h"

/* 外部驱动接口 */
extern int gpio_write(uint8_t port, uint8_t pin, uint8_t level);
extern int gpio_read(uint8_t port, uint8_t pin);
extern void os_delay_ms(uint32_t ms);
extern uint32_t os_get_tick(void);

/* ================================================================
 * 执行设备控制上下文
 * ================================================================ */
typedef struct {
    actuator_type_t type;
    device_state_t  state;
    uint8_t         control_mode;   /* 0=手动, 1=自动, 2=远程 */
    uint32_t        runtime_ms;     /* 累计运行时间 */
    uint32_t        last_on_time;   /* 最近一次开启时间 */
    uint8_t         fault_count;    /* 故障计数 */
    uint8_t         max_runtime_s;  /* 最大连续运行时间(秒) - 保护 */
    struct {
        uint8_t     port;
        uint8_t     pin;
    } hw;                           /* 硬件引脚映射 */
} actuator_ctx_t;

/* ================================================================
 * 设备表
 * ================================================================ */
static actuator_ctx_t g_actuators[] = {
    { ACTUATOR_PUMP,    DEVICE_OFF, 0, 0, 0, 0, 30,  {1, 12} },  /* 水泵: PB12 */
    { ACTUATOR_CURTAIN, DEVICE_OFF, 0, 0, 0, 0, 60,  {1, 13} },  /* 遮阳帘: PB13 */
    { ACTUATOR_FAN,     DEVICE_OFF, 0, 0, 0, 0, 120, {1, 14} },  /* 风扇: PB14 */
    { ACTUATOR_BUZZER,  DEVICE_OFF, 0, 0, 0, 0, 5,   {1, 0 } },  /* 蜂鸣器: PB0 */
};

#define ACTUATOR_COUNT  (sizeof(g_actuators) / sizeof(g_actuators[0]))

/* ================================================================
 * 执行设备控制
 * ================================================================ */

/**
 * @brief   开启执行设备
 */
int actuator_turn_on(actuator_type_t type)
{
    for (uint8_t i = 0; i < ACTUATOR_COUNT; i++) {
        if (g_actuators[i].type == type) {
            int ret = gpio_write(g_actuators[i].hw.port,
                                 g_actuators[i].hw.pin, 1);
            if (ret == ERR_OK) {
                g_actuators[i].state = DEVICE_ON;
                g_actuators[i].last_on_time = os_get_tick();
            }
            return ret;
        }
    }
    return ERR_INVALID_PARAM;
}

/**
 * @brief   关闭执行设备
 */
int actuator_turn_off(actuator_type_t type)
{
    for (uint8_t i = 0; i < ACTUATOR_COUNT; i++) {
        if (g_actuators[i].type == type) {
            int ret = gpio_write(g_actuators[i].hw.port,
                                 g_actuators[i].hw.pin, 0);
            if (ret == ERR_OK) {
                /* 累计运行时间 */
                if (g_actuators[i].state == DEVICE_ON) {
                    g_actuators[i].runtime_ms +=
                        (os_get_tick() - g_actuators[i].last_on_time);
                }
                g_actuators[i].state = DEVICE_OFF;
            }
            return ret;
        }
    }
    return ERR_INVALID_PARAM;
}

/**
 * @brief   获取设备状态
 */
device_state_t actuator_get_state(actuator_type_t type)
{
    for (uint8_t i = 0; i < ACTUATOR_COUNT; i++) {
        if (g_actuators[i].type == type) {
            return g_actuators[i].state;
        }
    }
    return DEVICE_FAULT;
}

/**
 * @brief   设备运行时间保护
 * @note    防止设备长时间运行导致损坏
 */
void actuator_runtime_protection(void)
{
    uint32_t now = os_get_tick();

    for (uint8_t i = 0; i < ACTUATOR_COUNT; i++) {
        if (g_actuators[i].state == DEVICE_ON) {
            uint32_t elapsed = (now - g_actuators[i].last_on_time) / 1000;
            if (elapsed > g_actuators[i].max_runtime_s) {
                /* 超时强制关闭 */
                actuator_turn_off(g_actuators[i].type);
            }
        }
    }
}

/**
 * @brief   初始化所有执行设备
 */
int actuator_init_all(void)
{
    for (uint8_t i = 0; i < ACTUATOR_COUNT; i++) {
        /* 确保所有设备默认关闭 */
        gpio_write(g_actuators[i].hw.port, g_actuators[i].hw.pin, 0);
        g_actuators[i].state = DEVICE_OFF;
    }
    return ERR_OK;
}
