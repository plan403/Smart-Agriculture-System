/**
 * @file    pin_config.h
 * @brief   硬件引脚定义配置
 * @note    根据实际硬件原理图修改引脚映射
 */

#ifndef __PIN_CONFIG_H__
#define __PIN_CONFIG_H__

#include "system_config.h"

/* ================================================================
 * STM32 平台引脚定义
 * ================================================================ */
#if TARGET_PLATFORM == PLATFORM_STM32

/* ---- GPIO ---- */
#define BUZZER_PIN              GPIO_PIN_0
#define BUZZER_PORT             GPIOB
#define LED_STATUS_PIN          GPIO_PIN_1
#define LED_STATUS_PORT         GPIOB
#define KEY_MANUAL_PIN          GPIO_PIN_2
#define KEY_MANUAL_PORT         GPIOB

/* ---- I2C1: 温湿度传感器 + 光照传感器 ---- */
#define I2C1_SCL_PIN            GPIO_PIN_6
#define I2C1_SCL_PORT           GPIOB
#define I2C1_SDA_PIN            GPIO_PIN_7
#define I2C1_SDA_PORT           GPIOB

/* ---- I2C2: CO2传感器 ---- */
#define I2C2_SCL_PIN            GPIO_PIN_10
#define I2C2_SCL_PORT           GPIOB
#define I2C2_SDA_PIN            GPIO_PIN_11
#define I2C2_SDA_PORT           GPIOB

/* ---- SPI1: 土壤湿度传感器 ---- */
#define SPI1_SCK_PIN            GPIO_PIN_5
#define SPI1_SCK_PORT           GPIOA
#define SPI1_MISO_PIN           GPIO_PIN_6
#define SPI1_MISO_PORT          GPIOA
#define SPI1_MOSI_PIN           GPIO_PIN_7
#define SPI1_MOSI_PORT          GPIOA
#define SPI1_CS_SOIL_PIN        GPIO_PIN_4
#define SPI1_CS_SOIL_PORT       GPIOA

/* ---- UART1: 调试串口 ---- */
#define UART1_TX_PIN            GPIO_PIN_9
#define UART1_TX_PORT           GPIOA
#define UART1_RX_PIN            GPIO_PIN_10
#define UART1_RX_PORT           GPIOA

/* ---- UART2: 云端通信 (WiFi/4G模块) ---- */
#define UART2_TX_PIN            GPIO_PIN_2
#define UART2_TX_PORT           GPIOA
#define UART2_RX_PIN            GPIO_PIN_3
#define UART2_RX_PORT           GPIOA

/* ---- 执行设备控制引脚 ---- */
#define PUMP_CTRL_PIN           GPIO_PIN_12
#define PUMP_CTRL_PORT          GPIOB
#define CURTAIN_CTRL_PIN        GPIO_PIN_13
#define CURTAIN_CTRL_PORT       GPIOB
#define FAN_CTRL_PIN            GPIO_PIN_14
#define FAN_CTRL_PORT           GPIOB

/* ================================================================
 * ESP32 平台引脚定义
 * ================================================================ */
#elif TARGET_PLATFORM == PLATFORM_ESP32

/* ---- GPIO ---- */
#define BUZZER_PIN              25
#define LED_STATUS_PIN          26
#define KEY_MANUAL_PIN          27

/* ---- I2C: 温湿度 + 光照 + CO2 ---- */
#define I2C_SCL_PIN             22
#define I2C_SDA_PIN             21

/* ---- SPI: 土壤湿度传感器 ---- */
#define SPI_SCK_PIN             18
#define SPI_MISO_PIN            19
#define SPI_MOSI_PIN            23
#define SPI_CS_SOIL_PIN         5

/* ---- UART1: 调试串口 ---- */
#define UART1_TX_PIN            1
#define UART1_RX_PIN            3

/* ---- UART2: 云端通信 ---- */
#define UART2_TX_PIN            17
#define UART2_RX_PIN            16

/* ---- 执行设备控制引脚 ---- */
#define PUMP_CTRL_PIN           32
#define CURTAIN_CTRL_PIN        33
#define FAN_CTRL_PIN            14

#endif /* TARGET_PLATFORM */

/* ================================================================
 * I2C 设备地址
 * ================================================================ */
#define I2C_ADDR_TEMP_HUMI      0x44    /* SHT30 温湿度传感器 */
#define I2C_ADDR_LIGHT          0x23    /* BH1750 光照传感器 */
#define I2C_ADDR_CO2            0x61    /* SCD30 CO2传感器 */

/* ================================================================
 * SPI 设备片选定义
 * ================================================================ */
#define SPI_CS_SOIL_MOISTURE    0       /* 土壤湿度传感器片选 */

#endif /* __PIN_CONFIG_H__ */
