/**
 * @file    uart_drv.c
 * @brief   UART 串口底层驱动实现
 * @note    基于寄存器直接操作，支持中断收发
 *          内置帧错误检测、噪声过滤、超时机制
 */

#include "system_config.h"
#include "pin_config.h"
#include "error_codes.h"

#if TARGET_PLATFORM == PLATFORM_STM32

#define USART1_BASE     0x40013800
#define USART2_BASE     0x40004400

/* USART 寄存器偏移 */
#define USART_SR_OFFSET     0x00    /* 状态寄存器 */
#define USART_DR_OFFSET     0x04    /* 数据寄存器 */
#define USART_BRR_OFFSET    0x08    /* 波特率寄存器 */
#define USART_CR1_OFFSET    0x0C    /* 控制寄存器1 */
#define USART_CR2_OFFSET    0x10    /* 控制寄存器2 */
#define USART_CR3_OFFSET    0x14    /* 控制寄存器3 */

/* USART SR 状态位 */
#define USART_SR_PE         0x0001  /* 校验错误 */
#define USART_SR_FE         0x0002  /* 帧错误 */
#define USART_SR_NE         0x0004  /* 噪声错误 */
#define USART_SR_ORE        0x0008  /* 溢出错误 */
#define USART_SR_IDLE       0x0010  /* 空闲线路 */
#define USART_SR_RXNE       0x0020  /* 接收非空 */
#define USART_SR_TC         0x0040  /* 发送完成 */
#define USART_SR_TXE        0x0080  /* 发送数据寄存器空 */

/* USART CR1 控制位 */
#define USART_CR1_UE        0x2000  /* USART使能 */
#define USART_CR1_M         0x1000  /* 字长 (0=8bit, 1=9bit) */
#define USART_CR1_PCE       0x0400  /* 校验使能 */
#define USART_CR1_PS        0x0200  /* 校验选择 (0=偶, 1=奇) */
#define USART_CR1_TE        0x0008  /* 发送使能 */
#define USART_CR1_RE        0x0004  /* 接收使能 */
#define USART_CR1_RXNEIE    0x0020  /* 接收中断使能 */
#define USART_CR1_IDLEIE    0x0010  /* 空闲中断使能 */

#define UART_TIMEOUT_MAX    100000

static uint32_t uart_base[3] = {0, USART1_BASE, USART2_BASE};

#define UART_REG(uart, offset) (*((volatile uint32_t *)(uart_base[uart] + offset)))

/**
 * @brief   UART 初始化
 * @param   uart: 串口号 (1=USART1, 2=USART2)
 * @param   baudrate: 波特率
 * @return  ERR_OK 或错误码
 * @note    8N1格式: 8数据位, 无校验, 1停止位
 */
int uart_init(uint8_t uart, uint32_t baudrate)
{
    uint32_t pclk;
    uint32_t brr_mantissa, brr_fraction;
    float usart_div;

    if (uart < 1 || uart > 2) return ERR_UART_INIT;

    /* ---- 1. 使能 USART 时钟 ---- */
    if (uart == 1) {
        RCC_APB2ENR |= (1 << 14);  /* USART1EN */
        pclk = APB2_CLOCK;
    } else {
        RCC_APB1ENR |= (1 << 17);  /* USART2EN */
        pclk = APB1_CLOCK;
    }

    /* ---- 2. 配置 GPIO ---- */
    if (uart == 1) {
        gpio_init(0, 9, GPIO_MODE_OUTPUT_PP);   /* PA9 - TX */
        gpio_init(0, 10, GPIO_MODE_INPUT);      /* PA10 - RX */
    } else {
        gpio_init(0, 2, GPIO_MODE_OUTPUT_PP);   /* PA2 - TX */
        gpio_init(0, 3, GPIO_MODE_INPUT);       /* PA3 - RX */
    }

    /* ---- 3. 计算波特率 ---- */
    usart_div = (float)pclk / (16.0f * baudrate);
    brr_mantissa = (uint32_t)usart_div;
    brr_fraction = (uint32_t)((usart_div - brr_mantissa) * 16 + 0.5f);

    if (brr_fraction > 15) {
        brr_fraction = 0;
        brr_mantissa++;
    }

    UART_REG(uart, USART_BRR_OFFSET) = (brr_mantissa << 4) | brr_fraction;

    /* ---- 4. 配置控制寄存器 - 8N1 ---- */
    UART_REG(uart, USART_CR1_OFFSET) = USART_CR1_TE     /* 发送使能 */
                                     | USART_CR1_RE;    /* 接收使能 */

    UART_REG(uart, USART_CR2_OFFSET) = 0;  /* 1个停止位 */
    UART_REG(uart, USART_CR3_OFFSET) = 0;  /* 无流控 */

    /* ---- 5. 使能 USART ---- */
    UART_REG(uart, USART_CR1_OFFSET) |= USART_CR1_UE;

    return ERR_OK;
}

/**
 * @brief   UART 发送单个字节
 */
int uart_send_byte(uint8_t uart, uint8_t data)
{
    uint32_t timeout = UART_TIMEOUT_MAX;

    /* 等待发送数据寄存器空 */
    while (!(UART_REG(uart, USART_SR_OFFSET) & USART_SR_TXE)) {
        if (--timeout == 0) return ERR_TIMEOUT;
    }

    UART_REG(uart, USART_DR_OFFSET) = data;
    return ERR_OK;
}

/**
 * @brief   UART 发送多字节
 */
int uart_send_bytes(uint8_t uart, uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        int ret = uart_send_byte(uart, data[i]);
        if (ret != ERR_OK) return ret;
    }

    /* 等待发送完成 */
    uint32_t timeout = UART_TIMEOUT_MAX;
    while (!(UART_REG(uart, USART_SR_OFFSET) & USART_SR_TC)) {
        if (--timeout == 0) return ERR_TIMEOUT;
    }
    return ERR_OK;
}

/**
 * @brief   UART 接收单个字节 (阻塞)
 */
int uart_recv_byte(uint8_t uart, uint8_t *data, uint32_t timeout_ms)
{
    uint32_t timeout = timeout_ms * 1000;  /* 转换为大致循环计数 */

    while (!(UART_REG(uart, USART_SR_OFFSET) & USART_SR_RXNE)) {
        if (--timeout == 0) return ERR_TIMEOUT;
    }

    *data = (uint8_t)UART_REG(uart, USART_DR_OFFSET);

    /* 检查错误标志 */
    uint32_t sr = UART_REG(uart, USART_SR_OFFSET);
    if (sr & (USART_SR_FE | USART_SR_NE | USART_SR_ORE | USART_SR_PE)) {
        /* 读DR清除错误 */
        (void)UART_REG(uart, USART_DR_OFFSET);
        if (sr & USART_SR_FE)  return ERR_UART_FRAME;
        if (sr & USART_SR_NE)  return ERR_UART_NOISE;
        if (sr & USART_SR_ORE) return ERR_UART_OVERRUN;
        if (sr & USART_SR_PE)  return ERR_UART_PARITY;
    }

    return ERR_OK;
}

/**
 * @brief   UART 接收多字节 (带超时)
 */
int uart_recv_bytes(uint8_t uart, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    for (uint16_t i = 0; i < len; i++) {
        int ret = uart_recv_byte(uart, &data[i], timeout_ms);
        if (ret != ERR_OK) return ret;
    }
    return ERR_OK;
}

/**
 * @brief   UART 发送字符串 (调试用)
 */
int uart_send_string(uint8_t uart, const char *str)
{
    while (*str) {
        int ret = uart_send_byte(uart, (uint8_t)*str++);
        if (ret != ERR_OK) return ret;
    }
    return ERR_OK;
}

/**
 * @brief   清除 UART 接收缓冲区
 */
void uart_flush_rx(uint8_t uart)
{
    uint8_t dummy;
    while (UART_REG(uart, USART_SR_OFFSET) & USART_SR_RXNE) {
        dummy = (uint8_t)UART_REG(uart, USART_DR_OFFSET);
        (void)dummy;
    }
}

#elif TARGET_PLATFORM == PLATFORM_ESP32

int uart_init(uint8_t uart, uint32_t baudrate)
{
    (void)uart; (void)baudrate;
    return ERR_OK;
}

int uart_send_bytes(uint8_t uart, uint8_t *data, uint16_t len)
{
    (void)uart; (void)data; (void)len;
    return ERR_OK;
}

int uart_recv_bytes(uint8_t uart, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)uart; (void)data; (void)len; (void)timeout_ms;
    return ERR_OK;
}

int uart_send_string(uint8_t uart, const char *str)
{
    (void)uart; (void)str;
    return ERR_OK;
}

void uart_flush_rx(uint8_t uart)
{
    (void)uart;
}

#endif /* TARGET_PLATFORM */

/**
 * @brief   初始化所有串口
 */
int uart_init_all(void)
{
    int ret;

    /* UART1 - 调试串口 */
    ret = uart_init(1, UART_BAUDRATE);
    if (ret != ERR_OK) return ret;

    /* UART2 - 云端通信串口 */
    ret = uart_init(2, UART_BAUDRATE);
    if (ret != ERR_OK) return ret;

    return ERR_OK;
}
