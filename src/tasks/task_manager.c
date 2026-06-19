/**
 * @file    task_manager.c
 * @brief   LiteOS 多任务管理器
 * @note    负责系统所有任务的创建、调度、同步
 *          基于信号量和消息队列实现任务间通信
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"

/* ================================================================
 * LiteOS 内核API (兼容 OpenHarmony LiteOS 接口)
 * ================================================================ */

/* ---- 任务控制块简化定义 ---- */
typedef struct {
    uint32_t        *stack_top;         /* 栈顶指针 */
    uint32_t        *stack_base;        /* 栈底指针 */
    uint32_t        stack_size;         /* 栈大小 (字) */
    uint8_t         priority;           /* 任务优先级 */
    uint8_t         state;              /* 任务状态 */
    uint8_t         task_id;            /* 任务ID */
    void            (*entry)(void *);   /* 任务入口函数 */
    void            *arg;               /* 任务参数 */
    uint32_t        wakeup_tick;        /* 唤醒滴答 */
} os_task_t;

/* ---- 信号量简化定义 ---- */
typedef struct {
    int32_t         count;              /* 信号量计数 */
    uint8_t         max_count;          /* 最大计数 */
    uint8_t         owner_task;         /* 当前持有者 */
} os_sem_t;

/* ---- 消息队列简化定义 ---- */
typedef struct {
    void            *buffer;            /* 队列缓冲区 */
    uint16_t        msg_size;           /* 单条消息大小 */
    uint16_t        max_msgs;           /* 最大消息数 */
    uint16_t        head;               /* 队头索引 */
    uint16_t        tail;               /* 队尾索引 */
    uint16_t        count;              /* 当前消息数 */
} os_queue_t;

/* ================================================================
 * 全局任务与同步对象
 * ================================================================ */
static os_task_t g_tasks[OS_MAX_TASKS];
static uint8_t   g_task_count = 0;

/* ---- 信号量 ---- */
static os_sem_t  g_sem_sensor_data;     /* 传感器数据互斥访问 */
static os_sem_t  g_sem_device_ctrl;     /* 设备控制互斥访问 */
static os_sem_t  g_sem_uart_tx;         /* 串口发送互斥 */

/* ---- 消息队列 ---- */
static os_queue_t g_queue_sensor;       /* 传感器数据队列 */
static os_queue_t g_queue_alarm;        /* 预警消息队列 */
static os_queue_t g_queue_command;      /* 指令队列 */
static os_queue_t g_queue_report;       /* 上报数据队列 */

/* ---- 系统滴答计数器 ---- */
static volatile uint32_t g_sys_tick = 0;

/* ================================================================
 * 内核基础函数
 * ================================================================ */

/**
 * @brief   系统滴答中断服务 (1ms周期)
 */
void os_tick_handler(void)
{
    g_sys_tick++;
}

/**
 * @brief   获取系统滴答
 */
uint32_t os_get_tick(void)
{
    return g_sys_tick;
}

/**
 * @brief   任务延时 (ms)
 */
void os_delay_ms(uint32_t ms)
{
    uint32_t wake_tick = g_sys_tick + ms;
    while (g_sys_tick < wake_tick) {
        __asm__ volatile ("nop");  /* 低功耗可替换为 WFI */
    }
}

/* ================================================================
 * 信号量操作
 * ================================================================ */

/**
 * @brief   创建信号量
 */
int os_sem_create(os_sem_t *sem, uint8_t max_count, uint8_t init_count)
{
    if (sem == NULL) return ERR_SEM_CREATE;

    sem->count = init_count;
    sem->max_count = max_count;
    sem->owner_task = 0xFF;
    return ERR_OK;
}

/**
 * @brief   获取信号量 (带超时)
 */
int os_sem_acquire(os_sem_t *sem, uint32_t timeout_ms)
{
    uint32_t start_tick = g_sys_tick;

    if (sem == NULL) return ERR_INVALID_PARAM;

    while (sem->count <= 0) {
        if ((g_sys_tick - start_tick) >= timeout_ms) {
            return ERR_SEM_TIMEOUT;
        }
        __asm__ volatile ("nop");
    }

    sem->count--;
    return ERR_OK;
}

/**
 * @brief   释放信号量
 */
int os_sem_release(os_sem_t *sem)
{
    if (sem == NULL) return ERR_INVALID_PARAM;

    if (sem->count < sem->max_count) {
        sem->count++;
    }
    return ERR_OK;
}

/* ================================================================
 * 消息队列操作
 * ================================================================ */

/**
 * @brief   创建消息队列
 */
int os_queue_create(os_queue_t *queue, void *buffer, uint16_t msg_size, uint16_t max_msgs)
{
    if (queue == NULL || buffer == NULL) return ERR_QUEUE_CREATE;

    queue->buffer = buffer;
    queue->msg_size = msg_size;
    queue->max_msgs = max_msgs;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    return ERR_OK;
}

/**
 * @brief   向队列发送消息
 */
int os_queue_send(os_queue_t *queue, void *data, uint32_t timeout_ms)
{
    uint32_t start_tick = g_sys_tick;

    if (queue == NULL || data == NULL) return ERR_INVALID_PARAM;

    /* 等待队列有空间 */
    while (queue->count >= queue->max_msgs) {
        if ((g_sys_tick - start_tick) >= timeout_ms) {
            return ERR_QUEUE_FULL;
        }
        __asm__ volatile ("nop");
    }

    /* 拷贝数据到队尾 */
    uint8_t *dest = (uint8_t *)queue->buffer + (queue->tail * queue->msg_size);
    for (uint16_t i = 0; i < queue->msg_size; i++) {
        dest[i] = ((uint8_t *)data)[i];
    }

    queue->tail = (queue->tail + 1) % queue->max_msgs;
    queue->count++;
    return ERR_OK;
}

/**
 * @brief   从队列接收消息
 */
int os_queue_recv(os_queue_t *queue, void *data, uint32_t timeout_ms)
{
    uint32_t start_tick = g_sys_tick;

    if (queue == NULL || data == NULL) return ERR_INVALID_PARAM;

    /* 等待队列有数据 */
    while (queue->count == 0) {
        if ((g_sys_tick - start_tick) >= timeout_ms) {
            return ERR_QUEUE_EMPTY;
        }
        __asm__ volatile ("nop");
    }

    /* 从队头拷贝数据 */
    uint8_t *src = (uint8_t *)queue->buffer + (queue->head * queue->msg_size);
    for (uint16_t i = 0; i < queue->msg_size; i++) {
        ((uint8_t *)data)[i] = src[i];
    }

    queue->head = (queue->head + 1) % queue->max_msgs;
    queue->count--;
    return ERR_OK;
}

/* ================================================================
 * 任务管理
 * ================================================================ */

/**
 * @brief   创建任务
 * @param   entry:    任务入口函数
 * @param   arg:      任务参数
 * @param   priority: 优先级 (数值越小越高)
 * @param   stack_size: 栈大小 (字)
 * @return  任务ID (0xFF表示失败)
 */
uint8_t os_task_create(void (*entry)(void *), void *arg, uint8_t priority, uint32_t stack_size)
{
    if (g_task_count >= OS_MAX_TASKS || entry == NULL) {
        return 0xFF;
    }

    uint8_t id = g_task_count;
    os_task_t *task = &g_tasks[id];

    /* 分配任务栈 */
    task->stack_size = (stack_size < OS_MIN_STACK_SIZE) ? OS_MIN_STACK_SIZE : stack_size;
    task->stack_base = (uint32_t *)malloc(task->stack_size * sizeof(uint32_t));
    if (task->stack_base == NULL) return 0xFF;

    task->stack_top = task->stack_base + task->stack_size - 16;  /* 预留上下文空间 */
    task->priority = priority;
    task->state = 0;  /* 就绪 */
    task->entry = entry;
    task->arg = arg;
    task->task_id = id;
    task->wakeup_tick = 0;

    g_task_count++;
    return id;
}

/**
 * @brief   启动任务调度器
 * @note    简化版调度器，按优先级轮询执行
 */
void os_scheduler_start(void)
{
    while (1) {
        uint8_t highest_prio_task = 0xFF;
        uint8_t highest_prio = 0xFF;

        /* 找出最高优先级就绪任务 */
        for (uint8_t i = 0; i < g_task_count; i++) {
            os_task_t *task = &g_tasks[i];

            /* 检查延时是否到期 */
            if (task->state == 1) {  /* 阻塞状态 */
                if (g_sys_tick >= task->wakeup_tick) {
                    task->state = 0;  /* 恢复就绪 */
                }
            }

            if (task->state == 0 && task->priority < highest_prio) {
                highest_prio = task->priority;
                highest_prio_task = i;
            }
        }

        /* 执行最高优先级任务 */
        if (highest_prio_task != 0xFF) {
            os_task_t *task = &g_tasks[highest_prio_task];
            task->state = 2;  /* 运行中 */
            task->entry(task->arg);
            task->state = 0;  /* 恢复就绪 */
        }

        /* 无就绪任务时进入低功耗 */
        __asm__ volatile ("wfi");
    }
}

/* ================================================================
 * 任务管理器初始化 (创建所有同步对象)
 * ================================================================ */

/* ---- 消息缓冲区 ---- */
static message_t g_sensor_msg_buf[8];
static alarm_info_t g_alarm_msg_buf[16];
static device_cmd_t g_cmd_msg_buf[8];
static cloud_report_t g_report_msg_buf[4];

/**
 * @brief   初始化所有信号量和消息队列
 */
int task_manager_init(void)
{
    int ret;

    /* 创建信号量 */
    ret = os_sem_create(&g_sem_sensor_data, 1, 1);
    if (ret != ERR_OK) return ret;

    ret = os_sem_create(&g_sem_device_ctrl, 1, 1);
    if (ret != ERR_OK) return ret;

    ret = os_sem_create(&g_sem_uart_tx, 1, 1);
    if (ret != ERR_OK) return ret;

    /* 创建消息队列 */
    ret = os_queue_create(&g_queue_sensor,
                          g_sensor_msg_buf,
                          sizeof(message_t), 8);
    if (ret != ERR_OK) return ret;

    ret = os_queue_create(&g_queue_alarm,
                          g_alarm_msg_buf,
                          sizeof(alarm_info_t), 16);
    if (ret != ERR_OK) return ret;

    ret = os_queue_create(&g_queue_command,
                          g_cmd_msg_buf,
                          sizeof(device_cmd_t), 8);
    if (ret != ERR_OK) return ret;

    ret = os_queue_create(&g_queue_report,
                          g_report_msg_buf,
                          sizeof(cloud_report_t), 4);
    if (ret != ERR_OK) return ret;

    return ERR_OK;
}

/**
 * @brief   获取全局信号量/队列的引用
 */
os_sem_t* get_sem_sensor_data(void)  { return &g_sem_sensor_data; }
os_sem_t* get_sem_device_ctrl(void)  { return &g_sem_device_ctrl; }
os_sem_t* get_sem_uart_tx(void)      { return &g_sem_uart_tx; }
os_queue_t* get_queue_sensor(void)   { return &g_queue_sensor; }
os_queue_t* get_queue_alarm(void)    { return &g_queue_alarm; }
os_queue_t* get_queue_command(void)  { return &g_queue_command; }
os_queue_t* get_queue_report(void)   { return &g_queue_report; }
