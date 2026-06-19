/**
 * @file    alarm_task.c
 * @brief   异常检测与预警任务
 * @note    持续监测环境数据，触发多级阈值预警
 *          本地蜂鸣器告警 + 消息队列分发 + 联动设备自动调控
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"

/* 外部接口 */
extern int get_env_data(env_data_t *env);
extern int device_auto_control(alarm_info_t *alarm);
extern int buzzer_control(uint8_t on);
extern os_queue_t* get_queue_alarm(void);
extern os_queue_t* get_queue_report(void);
extern void os_delay_ms(uint32_t ms);
extern uint32_t os_get_tick(void);

/* ================================================================
 * 预警状态记录 (防抖与滞后)
 * ================================================================ */
typedef struct {
    alarm_type_t    type;
    uint8_t         is_active;          /* 当前是否活跃 */
    uint32_t        first_trigger;      /* 首次触发时间 */
    uint32_t        last_trigger;       /* 最后触发时间 */
    uint8_t         trigger_count;      /* 连续触发次数 */
    float           peak_value;         /* 峰值 */
} alarm_state_t;

static alarm_state_t g_alarm_states[10];
static uint32_t g_last_buzzer_beep = 0;

/* ================================================================
 * 阈值检查函数
 * ================================================================ */

/**
 * @brief   单阈值检查 (大于上限)
 */
static alarm_level_t check_high_threshold(float value, float threshold,
                                           float hysteresis)
{
    if (value > threshold) {
        return ALARM_CRITICAL;
    } else if (value > (threshold - hysteresis)) {
        return ALARM_WARNING;
    }
    return ALARM_NONE;
}

/**
 * @brief   单阈值检查 (小于下限)
 */
static alarm_level_t check_low_threshold(float value, float threshold,
                                          float hysteresis)
{
    if (value < threshold) {
        return ALARM_CRITICAL;
    } else if (value < (threshold + hysteresis)) {
        return ALARM_WARNING;
    }
    return ALARM_NONE;
}

/**
 * @brief   生成预警信息
 */
static void alarm_generate(alarm_type_t type, alarm_level_t level,
                            float current, float threshold, const char *msg)
{
    alarm_info_t alarm;
    alarm.type = type;
    alarm.level = level;
    alarm.current_value = current;
    alarm.threshold_value = threshold;
    alarm.timestamp = os_get_tick();

    /* 安全拷贝消息 */
    int i;
    for (i = 0; i < 63 && msg[i] != '\0'; i++) {
        alarm.message[i] = msg[i];
    }
    alarm.message[i] = '\0';

    /* 发送到预警队列 */
    os_queue_send(get_queue_alarm(), &alarm, 50);

    /* 触发设备自动联动 */
    if (level >= ALARM_WARNING) {
        device_auto_control(&alarm);
    }
}

/**
 * @brief   蜂鸣器告警
 * @note    根据预警级别发出不同频率的蜂鸣
 */
static void buzzer_alarm(alarm_level_t level)
{
    uint32_t now = os_get_tick();

    switch (level) {
        case ALARM_WARNING:
            /* 慢速蜂鸣: 500ms间隔 */
            if ((now - g_last_buzzer_beep) > 500) {
                buzzer_control(1);
                os_delay_ms(100);
                buzzer_control(0);
                g_last_buzzer_beep = now;
            }
            break;

        case ALARM_CRITICAL:
            /* 快速蜂鸣: 200ms间隔 */
            if ((now - g_last_buzzer_beep) > 200) {
                buzzer_control(1);
                os_delay_ms(100);
                buzzer_control(0);
                g_last_buzzer_beep = now;
            }
            break;

        default:
            buzzer_control(0);
            break;
    }
}

/**
 * @brief   防抖检查 - 避免瞬时干扰触发误告警
 * @param   alarm_type: 预警类型索引
 * @param   is_triggered: 本轮是否触发
 * @return  true=确认告警, false=过滤
 */
static bool alarm_debounce(uint8_t alarm_type, bool is_triggered)
{
    alarm_state_t *state = &g_alarm_states[alarm_type];

    if (is_triggered) {
        state->trigger_count++;
        if (state->trigger_count == 1) {
            state->first_trigger = os_get_tick();
        }
        state->last_trigger = os_get_tick();

        /* 需要连续3次触发才确认告警 (避免噪声干扰) */
        if (state->trigger_count >= 3) {
            return true;
        }
    } else {
        /* 未触发时逐渐降低计数 (滞后恢复) */
        if (state->trigger_count > 0) {
            state->trigger_count--;
        }
    }
    return false;
}

/* ================================================================
 * 综合预警检测
 * ================================================================ */

/**
 * @brief   执行一次完整的预警检测
 */
static void alarm_check_all(void)
{
    env_data_t env;
    int ret;

    ret = get_env_data(&env);
    if (ret != ERR_OK) return;

    /* ---- 温度检测 ---- */
    alarm_level_t temp_level = ALARM_NONE;
    if (env.temperature > TEMP_HIGH_THRESHOLD) {
        temp_level = check_high_threshold(env.temperature,
                                          TEMP_HIGH_THRESHOLD, 2.0f);
        if (alarm_debounce(0, temp_level >= ALARM_WARNING)) {
            alarm_generate(ALARM_TYPE_TEMP_HIGH, temp_level,
                          env.temperature, TEMP_HIGH_THRESHOLD,
                          "温度过高! 自动开启风扇降温");
        }
    } else if (env.temperature < TEMP_LOW_THRESHOLD) {
        temp_level = check_low_threshold(env.temperature,
                                         TEMP_LOW_THRESHOLD, 2.0f);
        if (alarm_debounce(0, temp_level >= ALARM_WARNING)) {
            alarm_generate(ALARM_TYPE_TEMP_LOW, temp_level,
                          env.temperature, TEMP_LOW_THRESHOLD,
                          "温度过低! 注意防冻");
        }
    } else {
        g_alarm_states[0].trigger_count = 0;  /* 恢复正常 */
    }

    /* ---- 湿度检测 ---- */
    if (env.humidity > HUMI_HIGH_THRESHOLD) {
        alarm_level_t level = check_high_threshold(env.humidity,
                                                    HUMI_HIGH_THRESHOLD, 5.0f);
        if (alarm_debounce(1, level >= ALARM_WARNING)) {
            alarm_generate(ALARM_TYPE_HUMI_HIGH, level,
                          env.humidity, HUMI_HIGH_THRESHOLD,
                          "湿度偏高! 开启通风除湿");
        }
    } else if (env.humidity < HUMI_LOW_THRESHOLD) {
        alarm_level_t level = check_low_threshold(env.humidity,
                                                   HUMI_LOW_THRESHOLD, 5.0f);
        if (alarm_debounce(1, level >= ALARM_WARNING)) {
            alarm_generate(ALARM_TYPE_HUMI_LOW, level,
                          env.humidity, HUMI_LOW_THRESHOLD,
                          "湿度偏低! 建议开启喷雾加湿");
        }
    } else {
        g_alarm_states[1].trigger_count = 0;
    }

    /* ---- 土壤湿度检测 ---- */
    if (env.soil_moisture < SOIL_DRY_THRESHOLD) {
        if (alarm_debounce(2, true)) {
            alarm_generate(ALARM_TYPE_SOIL_DRY, ALARM_CRITICAL,
                          env.soil_moisture, SOIL_DRY_THRESHOLD,
                          "土壤干旱! 自动开启水泵灌溉");
        }
    } else {
        g_alarm_states[2].trigger_count = 0;
    }

    /* ---- 光照检测 ---- */
    if (env.light_intensity > LIGHT_HIGH_THRESHOLD) {
        if (alarm_debounce(3, true)) {
            alarm_generate(ALARM_TYPE_LIGHT_HIGH, ALARM_WARNING,
                          env.light_intensity, LIGHT_HIGH_THRESHOLD,
                          "光照过强! 自动展开遮阳帘");
        }
    } else if (env.light_intensity < LIGHT_LOW_THRESHOLD) {
        if (alarm_debounce(3, true)) {
            alarm_generate(ALARM_TYPE_LIGHT_LOW, ALARM_WARNING,
                          env.light_intensity, LIGHT_LOW_THRESHOLD,
                          "光照不足! 自动收起遮阳帘");
        }
    } else {
        g_alarm_states[3].trigger_count = 0;
    }

    /* ---- CO2浓度检测 ---- */
    if (env.co2_concentration > CO2_HIGH_THRESHOLD) {
        if (alarm_debounce(4, true)) {
            alarm_generate(ALARM_TYPE_CO2_HIGH, ALARM_CRITICAL,
                          env.co2_concentration, CO2_HIGH_THRESHOLD,
                          "CO2浓度过高! 开启风扇通风");
        }
    } else if (env.co2_concentration < CO2_LOW_THRESHOLD) {
        if (alarm_debounce(4, true)) {
            alarm_generate(ALARM_TYPE_CO2_LOW, ALARM_INFO,
                          env.co2_concentration, CO2_LOW_THRESHOLD,
                          "CO2浓度偏低");
        }
    } else {
        g_alarm_states[4].trigger_count = 0;
    }
}

/* ================================================================
 * 预警任务入口
 * ================================================================ */

void alarm_task_entry(void *arg)
{
    (void)arg;

    /* 初始化预警状态 */
    for (int i = 0; i < 10; i++) {
        g_alarm_states[i].type = (alarm_type_t)(i + 1);
        g_alarm_states[i].is_active = 0;
        g_alarm_states[i].trigger_count = 0;
    }

    while (1) {
        /* 执行一次完整预警检测 */
        alarm_check_all();

        os_delay_ms(1000);  /* 每秒检测一次 */
    }
}
