/**
 * @file    gpio_drv.c
 * @brief   GPIO 底层驱动实现
 * @note    基于寄存器直接操作，不依赖HAL库
 *          适配 STM32/ESP32 双平台
 */

#include "system_config.h"
#include "pin_config.h"
#include "error_codes.h"

/* ================================================================
 * STM32 平台 GPIO 寄存器映射
 * ================================================================ */
#if TARGET_PLATFORM == PLATFORM_STM32

/* STM32F10x GPIO 寄存器基地址 */
#define GPIOA_BASE      0x40010800
#define GPIOB_BASE      0x40010C00
#define GPIOC_BASE      0x40011000

/* GPIO 寄存器偏移 */
#define GPIO_CRL_OFFSET     0x00    /* 端口配置低寄存器 */
#define GPIO_CRH_OFFSET     0x04    /* 端口配置高寄存器 */
#define GPIO_IDR_OFFSET     0x08    /* 输入数据寄存器 */
#define GPIO_ODR_OFFSET     0x0C    /* 输出数据寄存器 */
#define GPIO_BSRR_OFFSET    0x10    /* 位设置/清除寄存器 */
#define GPIO_BRR_OFFSET     0x14    /* 位清除寄存器 */

/* RCC 时钟寄存器 */
#define RCC_BASE            0x40021000
#define RCC_APB2ENR_OFFSET  0x18
#define RCC_APB2ENR          (*((volatile uint32_t *)(RCC_BASE + RCC_APB2ENR_OFFSET)))

/* ---- GPIO寄存器访问宏 ---- */
#define GPIOA_REG(offset)   (*((volatile uint32_t *)(GPIOA_BASE + offset)))
#define GPIOB_REG(offset)   (*((volatile uint32_t *)(GPIOB_BASE + offset)))
#define GPIOC_REG(offset)   (*((volatile uint32_t *)(GPIOC_BASE + offset)))

/* GPIO 模式配置 (CRL/CRH) */
#define GPIO_MODE_INPUT         0x04    /* 浮空输入 */
#define GPIO_MODE_INPUT_PU      0x08    /* 上拉输入 */
#define GPIO_MODE_OUTPUT_PP     0x03    /* 推挽输出 50MHz */
#define GPIO_MODE_OUTPUT_OD     0x07    /* 开漏输出 50MHz */

/* ---- 获取GPIO基地址 ---- */
static uint32_t gpio_get_base(uint8_t port)
{
    switch (port) {
        case 0: return GPIOA_BASE;
        case 1: return GPIOB_BASE;
        case 2: return GPIOC_BASE;
        default: return 0;
    }
}

/* ---- 使能GPIO时钟 ---- */
static void gpio_enable_clock(uint8_t port)
{
    RCC_APB2ENR |= (1 << (2 + port));  /* IOPAEN=bit2, IOPBEN=bit3, IOPCEN=bit4 */
    __asm__ volatile ("nop");           /* 等待时钟稳定 */
    __asm__ volatile ("nop");
}

/**
 * @brief   GPIO 初始化 (寄存器级配置)
 * @param   port: 端口号 (0=A, 1=B, 2=C)
 * @param   pin:  引脚号 (0-15)
 * @param   mode: 工作模式
 * @return  ERR_OK 或错误码
 */
int gpio_init(uint8_t port, uint8_t pin, uint8_t mode)
{
    uint32_t base, config_reg, shift;

    if (port > 2 || pin > 15) return ERR_INVALID_PARAM;

    base = gpio_get_base(port);
    if (base == 0) return ERR_GPIO_INIT;

    /* 使能 GPIO 时钟 */
    gpio_enable_clock(port);

    /* 配置引脚模式 */
    if (pin < 8) {
        config_reg = base + GPIO_CRL_OFFSET;
        shift = pin * 4;
    } else {
        config_reg = base + GPIO_CRH_OFFSET;
        shift = (pin - 8) * 4;
    }

    /* 清除原有配置，写入新模式 */
    (*((volatile uint32_t *)config_reg)) &= ~(0x0F << shift);
    (*((volatile uint32_t *)config_reg)) |= (mode << shift);

    return ERR_OK;
}

/**
 * @brief   GPIO 输出高/低电平
 * @param   port: 端口号
 * @param   pin:  引脚号
 * @param   level: 0=低电平, 1=高电平
 */
int gpio_write(uint8_t port, uint8_t pin, uint8_t level)
{
    uint32_t base = gpio_get_base(port);
    if (base == 0 || pin > 15) return ERR_GPIO_WRITE;

    if (level) {
        /* 使用 BSRR 寄存器置位 (bit0-15用于置位) */
        (*((volatile uint32_t *)(base + GPIO_BSRR_OFFSET))) = (1 << pin);
    } else {
        /* 使用 BRR 寄存器清除 (或 BSRR bit16-31用于清除) */
        (*((volatile uint32_t *)(base + GPIO_BRR_OFFSET))) = (1 << pin);
    }
    return ERR_OK;
}

/**
 * @brief   GPIO 读取输入电平
 * @param   port: 端口号
 * @param   pin:  引脚号
 * @return  0或1 (失败返回-1)
 */
int gpio_read(uint8_t port, uint8_t pin)
{
    uint32_t base = gpio_get_base(port);
    if (base == 0 || pin > 15) return -1;

    return ((*((volatile uint32_t *)(base + GPIO_IDR_OFFSET))) >> pin) & 0x01;
}

/* ================================================================
 * ESP32 平台 GPIO 实现
 * ================================================================ */
#elif TARGET_PLATFORM == PLATFORM_ESP32

/* ESP32 GPIO 寄存器基地址 */
#define ESP32_GPIO_BASE             0x3FF44000
#define ESP32_GPIO_ENABLE_REG       ((volatile uint32_t *)(ESP32_GPIO_BASE + 0x20))
#define ESP32_GPIO_OUT_REG          ((volatile uint32_t *)(ESP32_GPIO_BASE + 0x04))
#define ESP32_GPIO_OUT_W1TS_REG     ((volatile uint32_t *)(ESP32_GPIO_BASE + 0x08))
#define ESP32_GPIO_OUT_W1TC_REG     ((volatile uint32_t *)(ESP32_GPIO_BASE + 0x0C))
#define ESP32_GPIO_IN_REG           ((volatile uint32_t *)(ESP32_GPIO_BASE + 0x3C))
#define ESP32_IO_MUX_BASE           0x3FF49000

int gpio_init(uint8_t port, uint8_t pin, uint8_t mode)
{
    if (pin > 39) return ERR_INVALID_PARAM;

    /* 使能 GPIO 输出 */
    if (mode == GPIO_MODE_OUTPUT_PP || mode == GPIO_MODE_OUTPUT_OD) {
        *ESP32_GPIO_ENABLE_REG |= (1 << pin);
    } else {
        *ESP32_GPIO_ENABLE_REG &= ~(1 << pin);
    }
    return ERR_OK;
}

int gpio_write(uint8_t port, uint8_t pin, uint8_t level)
{
    if (pin > 39) return ERR_GPIO_WRITE;

    if (level) {
        *ESP32_GPIO_OUT_W1TS_REG = (1 << pin);
    } else {
        *ESP32_GPIO_OUT_W1TC_REG = (1 << pin);
    }
    return ERR_OK;
}

int gpio_read(uint8_t port, uint8_t pin)
{
    if (pin > 39) return -1;
    return ((*ESP32_GPIO_IN_REG) >> pin) & 0x01;
}

#endif /* TARGET_PLATFORM */

/* ================================================================
 * 高层 GPIO 封装函数
 * ================================================================ */

/**
 * @brief   初始化蜂鸣器、LED、按键等外设GPIO
 */
int gpio_init_peripherals(void)
{
    int ret;

    /* 蜂鸣器 - 推挽输出 */
    ret = gpio_init(1, 0, GPIO_MODE_OUTPUT_PP);
    if (ret != ERR_OK) return ret;
    gpio_write(1, 0, 0);  /* 默认关闭 */

    /* 状态指示灯 - 推挽输出 */
    ret = gpio_init(1, 1, GPIO_MODE_OUTPUT_PP);
    if (ret != ERR_OK) return ret;

    /* 手动按键 - 上拉输入 */
    ret = gpio_init(1, 2, GPIO_MODE_INPUT_PU);
    if (ret != ERR_OK) return ret;

    /* 水泵控制 - 推挽输出 */
    ret = gpio_init(1, 12, GPIO_MODE_OUTPUT_PP);
    if (ret != ERR_OK) return ret;
    gpio_write(1, 12, 0);

    /* 遮阳帘控制 - 推挽输出 */
    ret = gpio_init(1, 13, GPIO_MODE_OUTPUT_PP);
    if (ret != ERR_OK) return ret;
    gpio_write(1, 13, 0);

    /* 通风风扇控制 - 推挽输出 */
    ret = gpio_init(1, 14, GPIO_MODE_OUTPUT_PP);
    if (ret != ERR_OK) return ret;
    gpio_write(1, 14, 0);

    return ERR_OK;
}
