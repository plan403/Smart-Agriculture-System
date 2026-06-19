/**
 * @file    error_codes.c
 * @brief   错误码字符串转换实现
 */

#include "error_codes.h"

/**
 * @brief   错误码转可读字符串
 */
const char* error_to_string(uint8_t err_code)
{
    switch (err_code) {
        case ERR_OK:                    return "OK";
        case ERR_FAIL:                  return "General Failure";
        case ERR_TIMEOUT:               return "Timeout";
        case ERR_BUSY:                  return "Resource Busy";
        case ERR_INVALID_PARAM:         return "Invalid Parameter";
        case ERR_NO_MEMORY:             return "Out of Memory";
        case ERR_NOT_INITIALIZED:       return "Not Initialized";
        case ERR_ALREADY_INITIALIZED:   return "Already Initialized";

        case ERR_GPIO_INIT:             return "GPIO Init Failed";
        case ERR_GPIO_WRITE:            return "GPIO Write Failed";
        case ERR_GPIO_READ:             return "GPIO Read Failed";

        case ERR_I2C_INIT:              return "I2C Init Failed";
        case ERR_I2C_TIMING:            return "I2C Timing Error";
        case ERR_I2C_NACK:              return "I2C NACK";
        case ERR_I2C_ARBITRATION_LOST:  return "I2C Arbitration Lost";
        case ERR_I2C_BUS_ERROR:         return "I2C Bus Error";

        case ERR_SPI_INIT:              return "SPI Init Failed";
        case ERR_SPI_TIMING:            return "SPI Timing Error";
        case ERR_SPI_OVERRUN:           return "SPI Overrun";
        case ERR_SPI_PACKET_LOST:       return "SPI Packet Lost";
        case ERR_SPI_CRC_FAIL:          return "SPI CRC Failed";

        case ERR_UART_INIT:             return "UART Init Failed";
        case ERR_UART_FRAME:            return "UART Frame Error";
        case ERR_UART_PARITY:           return "UART Parity Error";
        case ERR_UART_OVERRUN:          return "UART Overrun";
        case ERR_UART_NOISE:            return "UART Noise Error";

        case ERR_SENSOR_NOT_FOUND:      return "Sensor Not Found";
        case ERR_SENSOR_READ_FAIL:      return "Sensor Read Failed";
        case ERR_SENSOR_DATA_INVALID:   return "Sensor Data Invalid";
        case ERR_SENSOR_CALIBRATION:    return "Sensor Calibration Failed";
        case ERR_SENSOR_DISCONNECTED:   return "Sensor Disconnected";

        case ERR_TASK_CREATE:           return "Task Create Failed";
        case ERR_STACK_OVERFLOW:        return "Task Stack Overflow";
        case ERR_SEM_TIMEOUT:           return "Semaphore Timeout";
        case ERR_QUEUE_FULL:            return "Queue Full";
        case ERR_QUEUE_EMPTY:           return "Queue Empty";

        case ERR_CLOUD_CONNECT:         return "Cloud Connect Failed";
        case ERR_CLOUD_SEND:            return "Cloud Send Failed";
        case ERR_CLOUD_RECV:            return "Cloud Receive Failed";
        case ERR_PROTOCOL_PARSE:        return "Protocol Parse Error";
        case ERR_CRC_MISMATCH:          return "CRC Mismatch";

        default:                        return "Unknown Error";
    }
}
