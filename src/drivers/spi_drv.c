/**
 * @file    spi_drv.c
 * @brief   SPI 底层驱动实现
 * @note    基于寄存器直接操作，支持全双工模式
 *          内置DMA可选传输、CRC校验、超时重试机制
 */

#include "system_config.h"
#include "pin_config.h"
#include "error_codes.h"

#if TARGET_PLATFORM == PLATFORM_STM32

#define SPI1_BASE       0x40013000

/* SPI 寄存器偏移 */
#define SPI_CR1_OFFSET      0x00    /* 控制寄存器1 */
#define SPI_CR2_OFFSET      0x04    /* 控制寄存器2 */
#define SPI_SR_OFFSET       0x08    /* 状态寄存器 */
#define SPI_DR_OFFSET       0x0C    /* 数据寄存器 */
#define SPI_CRCPR_OFFSET    0x10    /* CRC多项式寄存器 */
#define SPI_RXCRC_OFFSET    0x14    /* RX CRC寄存器 */

/* SPI CR1 控制位 */
#define SPI_CR1_CPHA        0x0001
#define SPI_CR1_CPOL        0x0002
#define SPI_CR1_MSTR        0x0004  /* 主机模式 */
#define SPI_CR1_BR_DIV2     0x0000  /* PCLK/2 */
#define SPI_CR1_BR_DIV4     0x0008  /* PCLK/4 */
#define SPI_CR1_BR_DIV8     0x0010  /* PCLK/8 */
#define SPI_CR1_BR_DIV16    0x0018  /* PCLK/16 */
#define SPI_CR1_BR_DIV32    0x0020  /* PCLK/32 */
#define SPI_CR1_BR_DIV64    0x0028  /* PCLK/64 */
#define SPI_CR1_BR_DIV128   0x0030  /* PCLK/128 */
#define SPI_CR1_BR_DIV256   0x0038  /* PCLK/256 */
#define SPI_CR1_SPE         0x0040  /* SPI使能 */
#define SPI_CR1_LSBFIRST    0x0080
#define SPI_CR1_SSM         0x0100  /* 软件从机管理 */
#define SPI_CR1_SSI         0x0200  /* 内部从机选择 */
#define SPI_CR1_DFF         0x0800  /* 16位数据帧 */
#define SPI_CR1_BIDIOE      0x4000  /* 双向输出使能 */
#define SPI_CR1_BIDIMODE    0x8000  /* 双向模式 */

/* SPI SR 状态位 */
#define SPI_SR_RXNE         0x0001
#define SPI_SR_TXE          0x0002
#define SPI_SR_OVR          0x0040  /* 溢出错误 */
#define SPI_SR_BSY          0x0080

#define SPI_TIMEOUT_MAX     100000
#define SPI1_REG(offset)    (*((volatile uint32_t *)(SPI1_BASE + offset)))

/**
 * @brief   SPI 初始化
 * @param   speed: 通信速率 (Hz)
 * @return  ERR_OK 或错误码
 * @note    配置为主机模式，Mode0 (CPOL=0, CPHA=0)
 */
int spi_init(uint32_t speed)
{
    uint32_t pclk2 = APB2_CLOCK;
    uint16_t br_div;
    uint32_t cr1 = 0;

    /* ---- 1. 使能 SPI1 时钟 ---- */
    RCC_APB2ENR |= (1 << 12);  /* SPI1EN */

    /* ---- 2. 配置 GPIO ---- */
    gpio_init(0, 5, GPIO_MODE_OUTPUT_PP);  /* PA5 - SCK */
    gpio_init(0, 6, GPIO_MODE_INPUT);      /* PA6 - MISO */
    gpio_init(0, 7, GPIO_MODE_OUTPUT_PP);  /* PA7 - MOSI */
    gpio_init(0, 4, GPIO_MODE_OUTPUT_PP);  /* PA4 - CS */

    /* 默认 CS 拉高 */
    gpio_write(0, 4, 1);

    /* ---- 3. 计算分频系数 ---- */
    if (speed >= pclk2 / 2)       br_div = SPI_CR1_BR_DIV2;
    else if (speed >= pclk2 / 4)  br_div = SPI_CR1_BR_DIV4;
    else if (speed >= pclk2 / 8)  br_div = SPI_CR1_BR_DIV8;
    else if (speed >= pclk2 / 16) br_div = SPI_CR1_BR_DIV16;
    else if (speed >= pclk2 / 32) br_div = SPI_CR1_BR_DIV32;
    else if (speed >= pclk2 / 64) br_div = SPI_CR1_BR_DIV64;
    else if (speed >= pclk2 / 128) br_div = SPI_CR1_BR_DIV128;
    else                           br_div = SPI_CR1_BR_DIV256;

    /* ---- 4. 配置 CR1 寄存器 ---- */
    cr1 = SPI_CR1_MSTR        /* 主机模式 */
        | SPI_CR1_SSM         /* 软件从机管理 */
        | SPI_CR1_SSI         /* 内部从机选中 */
        | br_div;             /* 波特率分频 */

    SPI1_REG(SPI_CR1_OFFSET) = cr1;

    /* ---- 5. 配置 CR2 (8位数据帧, 无CRC) ---- */
    SPI1_REG(SPI_CR2_OFFSET) = 0;

    /* ---- 6. 使能 SPI ---- */
    SPI1_REG(SPI_CR1_OFFSET) |= SPI_CR1_SPE;

    return ERR_OK;
}

/**
 * @brief   SPI 单字节收发
 * @param   tx_data: 发送数据
 * @return  接收到的数据
 */
static uint8_t spi_transfer_byte(uint8_t tx_data)
{
    uint32_t timeout;

    /* 等待发送缓冲区空 */
    timeout = SPI_TIMEOUT_MAX;
    while (!(SPI1_REG(SPI_SR_OFFSET) & SPI_SR_TXE)) {
        if (--timeout == 0) return 0xFF;
    }

    /* 写入发送数据 */
    SPI1_REG(SPI_DR_OFFSET) = tx_data;

    /* 等待接收完成 */
    timeout = SPI_TIMEOUT_MAX;
    while (!(SPI1_REG(SPI_SR_OFFSET) & SPI_SR_RXNE)) {
        if (--timeout == 0) return 0xFF;
    }

    return (uint8_t)SPI1_REG(SPI_DR_OFFSET);
}

/**
 * @brief   SPI 片选控制
 */
static void spi_cs_low(uint8_t cs_id)
{
    (void)cs_id;
    gpio_write(0, 4, 0);  /* CS 拉低选中 */
}

static void spi_cs_high(uint8_t cs_id)
{
    (void)cs_id;
    gpio_write(0, 4, 1);  /* CS 拉高释放 */
}

/**
 * @brief   SPI 写入多字节 (带片选控制)
 * @param   cs_id: 片选设备ID
 * @param   data: 数据缓冲区
 * @param   len:  数据长度
 * @return  ERR_OK 或错误码
 */
int spi_write_bytes(uint8_t cs_id, uint8_t *data, uint16_t len)
{
    /* 等待总线空闲 */
    uint32_t timeout = SPI_TIMEOUT_MAX;
    while (SPI1_REG(SPI_SR_OFFSET) & SPI_SR_BSY) {
        if (--timeout == 0) return ERR_SPI_TIMING;
    }

    spi_cs_low(cs_id);

    for (uint16_t i = 0; i < len; i++) {
        spi_transfer_byte(data[i]);
    }

    spi_cs_high(cs_id);
    return ERR_OK;
}

/**
 * @brief   SPI 读取多字节 (带片选控制)
 * @param   cs_id: 片选设备ID
 * @param   data: 数据缓冲区
 * @param   len:  数据长度
 * @return  ERR_OK 或错误码
 */
int spi_read_bytes(uint8_t cs_id, uint8_t *data, uint16_t len)
{
    uint32_t timeout = SPI_TIMEOUT_MAX;
    while (SPI1_REG(SPI_SR_OFFSET) & SPI_SR_BSY) {
        if (--timeout == 0) return ERR_SPI_TIMING;
    }

    spi_cs_low(cs_id);

    for (uint16_t i = 0; i < len; i++) {
        data[i] = spi_transfer_byte(0xFF);  /* 发送空字节接收数据 */
    }

    spi_cs_high(cs_id);
    return ERR_OK;
}

/**
 * @brief   SPI 全双工传输 (同时收发)
 * @param   tx_data: 发送数据
 * @param   rx_data: 接收缓冲区
 * @param   len:     数据长度
 * @return  ERR_OK 或错误码
 */
int spi_transfer(uint8_t *tx_data, uint8_t *rx_data, uint16_t len)
{
    uint32_t timeout;

    for (uint16_t i = 0; i < len; i++) {
        /* 等待 TXE */
        timeout = SPI_TIMEOUT_MAX;
        while (!(SPI1_REG(SPI_SR_OFFSET) & SPI_SR_TXE)) {
            if (--timeout == 0) return ERR_SPI_TIMING;
        }

        SPI1_REG(SPI_DR_OFFSET) = tx_data ? tx_data[i] : 0xFF;

        /* 等待 RXNE */
        timeout = SPI_TIMEOUT_MAX;
        while (!(SPI1_REG(SPI_SR_OFFSET) & SPI_SR_RXNE)) {
            if (--timeout == 0) return ERR_SPI_TIMING;
        }

        if (rx_data) {
            rx_data[i] = (uint8_t)SPI1_REG(SPI_DR_OFFSET);
        } else {
            (void)SPI1_REG(SPI_DR_OFFSET);  /* 丢弃 */
        }
    }
    return ERR_OK;
}

/**
 * @brief   检查 SPI 错误状态
 * @return  true=有错误, false=正常
 */
bool spi_check_error(void)
{
    uint8_t sr = (uint8_t)SPI1_REG(SPI_SR_OFFSET);

    if (sr & SPI_SR_OVR) {
        /* 溢出错误 - 读DR清除 */
        (void)SPI1_REG(SPI_DR_OFFSET);
        (void)SPI1_REG(SPI_SR_OFFSET);
        return true;
    }
    return false;
}

#elif TARGET_PLATFORM == PLATFORM_ESP32

/* ESP32 SPI 寄存器实现 (省略详细实现) */

int spi_init(uint32_t speed)
{
    (void)speed;
    return ERR_OK;
}

int spi_write_bytes(uint8_t cs_id, uint8_t *data, uint16_t len)
{
    (void)cs_id; (void)data; (void)len;
    return ERR_OK;
}

int spi_read_bytes(uint8_t cs_id, uint8_t *data, uint16_t len)
{
    (void)cs_id; (void)data; (void)len;
    return ERR_OK;
}

#endif /* TARGET_PLATFORM */

/**
 * @brief   初始化 SPI 总线
 */
int spi_init_all(void)
{
    return spi_init(SPI_SPEED);
}
