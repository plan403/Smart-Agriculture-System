/**
 * @file    control_task.c
 * @brief   设备控制任务 - 处理本地按键与云端指令
 * @note    接收指令队列消息，驱动执行设备
 *          支持手动按键控制与自动联动控制
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"
#include "pin_config.h"

/* 外部接口 */
extern int gpio_write(uint8_t port, uint8_t pin, uint8_t level);
extern int gpio_read(uint8_t port, uint8_t pin);
extern os_sem_t* get_sem_device_ctrl(void);
extern os_queue_t* get_queue_command(void);
extern void os_delay_ms(uint32_t ms);
extern uint32_t os_get_tick(void);

/* ================================================================
 * 设备状态记录
 * ================================================================ */
static struct {
    device_state_t state;
    uint8_t        is_manual;      /* 是否手动控制中 */
    uint32_t       last_toggle;    /* 上次切换时间 */
} g_devices[ACTUATOR_MAX];

/* ================================================================
 * 执行设备底层控制
 * ================================================================ */

/**
 * @brief   水泵控制
 * @param   on: 1=开启, 0=关闭
 */
int pump_control(uint8_t on)
{
    return gpio_write(1, 12, on ? 1 : 0);
}

/**
 * @brief   遮阳帘控制
 * @param   on: 1=展开(遮阳), 0=收起
 */
int curtain_control(uint8_t on)
{
    return gpio_write(1, 13, on ? 1 : 0);
}

/**
 * @brief   通风风扇控制
 * @param   on: 1=开启, 0=关闭
 */
int fan_control(uint8_t on)
{
    return gpio_write(1, 14, on ? 1 : 0);
}

/**
 * @brief   蜂鸣器控制
 */
int buzzer_control(uint8_t on)
{
    return gpio_write(1, 0, on ? 1 : 0);
}

/**
 * @brief   状态指示灯
 */
int led_set(uint8_t on)
{
    return gpio_write(1, 1, on ? 1 : 0);
}

/* ================================================================
 * 通用设备控制接口
 * ================================================================ */

/**
 * @brief   执行设备统一控制
 * @param   cmd: 设备控制指令
 * @return  ERR_OK 或错误码
 */
int device_execute(device_cmd_t *cmd)
{
    int ret = ERR_OK;
    os_sem_t *sem = get_sem_device_ctrl();

    if (cmd == NULL) return ERR_INVALID_PARAM;

    os_sem_acquire(sem, 100);

    switch (cmd->device_type) {
        case ACTUATOR_PUMP:
            ret = pump_control(cmd->command);
            break;
        case ACTUATOR_CURTAIN:
            ret = curtain_control(cmd->command);
            break;
        case ACTUATOR_FAN:
            ret = fan_control(cmd->command);
            break;
        case ACTUATOR_BUZZER:
            ret = buzzer_control(cmd->command);
            break;
        default:
            ret = ERR_INVALID_PARAM;
            break;
    }

    /* 更新设备状态 */
    if (ret == ERR_OK && cmd->device_type < ACTUATOR_MAX) {
        g_devices[cmd->device_type].state = cmd->command ? DEVICE_ON : DEVICE_OFF;
        g_devices[cmd->device_type].is_manual = cmd->is_manual;
        g_devices[cmd->device_type].last_toggle = os_get_tick();
    }

    os_sem_release(sem);
    return ret;
}

/**
 * @brief   获取设备当前状态
 */
device_state_t device_get_state(actuator_type_t type)
{
    if (type >= ACTUATOR_MAX) return DEVICE_FAULT;
    return g_devices[type].state;
}

/* ================================================================
 * 手动按键检测
 * ================================================================ */

static uint32_t g_last_key_press = 0;
static uint8_t  g_key_state = 0;

/**
 * @brief   检测手动按键 (带消抖)
 * @return  0=无按键, 1=短按, 2=长按
 */
static uint8_t key_scan(void)
{
    int level = gpio_read(1, 2);
    uint32_t now = os_get_tick();

    if (level == 0) {  /* 低电平 = 按键按下 */
        if (g_key_state == 0) {
            g_key_state = 1;
            g_last_key_press = now;
        } else if (g_key_state == 1 && (now - g_last_key_press) > 50) {
            /* 消抖 50ms */
            g_key_state = 2;
        } else if (g_key_state == 2 && (now - g_last_key_press) > 2000) {
            /* 长按 2s */
            return 2;
        }
    } else {
        if (g_key_state == 2 && (now - g_last_key_press) > 50) {
            g_key_state = 0;
            return 1;  /* 短按释放 */
        }
        g_key_state = 0;
    }
    return 0;
}

/* ================================================================
 * 自动联动控制
 * ================================================================ */

/**
 * @brief   根据预警信息自动控制设备
 * @note    当环境异常时自动联动对应执行设备
 */
int device_auto_control(alarm_info_t *alarm)
{
    device_cmd_t cmd;
    cmd.is_manual = 0;  /* 自动控制 */
    cmd.timestamp = os_get_tick();

    switch (alarm->type) {
        case ALARM_TYPE_TEMP_HIGH:
            /* 温度过高 -> 开启风扇 + 展开遮阳帘 */
            cmd.device_type = ACTUATOR_FAN;
            cmd.command = 1;
            device_execute(&cmd);

            cmd.device_type = ACTUATOR_CURTAIN;
            cmd.command = 1;
            device_execute(&cmd);
            break;

        case ALARM_TYPE_TEMP_LOW:
            /* 温度过低 -> 关闭风扇 + 收起遮阳帘 */
            cmd.device_type = ACTUATOR_FAN;
            cmd.command = 0;
            device_execute(&cmd);

            cmd.device_type = ACTUATOR_CURTAIN;
            cmd.command = 0;
            device_execute(&cmd);
            break;

        case ALARM_TYPE_SOIL_DRY:
            /* 土壤干旱 -> 开启水泵 */
            cmd.device_type = ACTUATOR_PUMP;
            cmd.command = 1;
            device_execute(&cmd);
            break;

        case ALARM_TYPE_LIGHT_HIGH:
            /* 光照过强 -> 展开遮阳帘 */
            cmd.device_type = ACTUATOR_CURTAIN;
            cmd.command = 1;
            device_execute(&cmd);
            break;

        case ALARM_TYPE_LIGHT_LOW:
            /* 光照不足 -> 收起遮阳帘 */
            cmd.device_type = ACTUATOR_CURTAIN;
            cmd.command = 0;
            device_execute(&cmd);
            break;

        case ALARM_TYPE_CO2_HIGH:
            /* CO2过高 -> 开启风扇通风 */
            cmd.device_type = ACTUATOR_FAN;
            cmd.command = 1;
            device_execute(&cmd);
            break;

        default:
            break;
    }
    return ERR_OK;
}

/* ================================================================
 * 设备控制任务入口
 * ================================================================ */

/**
 * @brief   设备控制任务
 * @note    处理来自指令队列的控制命令
 *          同时扫描手动按键
 */
void control_task_entry(void *arg)
{
    (void)arg;
    device_cmd_t cmd;

    while (1) {
        /* 1. 检查手动按键 */
        uint8_t key = key_scan();
        if (key == 1) {
            /* 短按: 切换水泵状态 */
            cmd.device_type = ACTUATOR_PUMP;
            cmd.command = (device_get_state(ACTUATOR_PUMP) == DEVICE_ON) ? 0 : 1;
            cmd.is_manual = 1;
            cmd.timestamp = os_get_tick();
            device_execute(&cmd);
        }

        /* 2. 处理指令队列 (非阻塞) */
        int ret = os_queue_recv(get_queue_command(), &cmd, 10);
        if (ret == ERR_OK) {
            device_execute(&cmd);
        }

        /* 3. LED状态指示 */
        /* 有设备开启时LED闪烁 */
        uint8_t any_on = 0;
        for (int i = 0; i < ACTUATOR_MAX; i++) {
            if (g_devices[i].state == DEVICE_ON) any_on = 1;
        }
        led_set(any_on);

        os_delay_ms(50);  /* 50ms 轮询周期 */
    }
}
