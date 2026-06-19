/**
 * @file    sensor_hub.c
 * @brief   传感器数据中心 - 统一管理所有传感器设备
 * @note    实现传感器即插即用、热接入管理
 *          提供标准化设备接入接口
 */

#include "system_config.h"
#include "common_types.h"
#include "error_codes.h"
#include "device_protocol.h"

/* ================================================================
 * 传感器设备表 (最多支持16个设备)
 * ================================================================ */
#define MAX_DEVICES     16

static device_interface_t g_device_table[MAX_DEVICES];
static uint8_t            g_device_count = 0;

/* ================================================================
 * 设备注册与注销
 * ================================================================ */

/**
 * @brief   注册新设备
 * @param   dev_if: 设备接口描述
 * @return  ERR_OK 或错误码
 */
int protocol_register_device(device_interface_t *dev_if)
{
    if (dev_if == NULL || g_device_count >= MAX_DEVICES) {
        return ERR_INVALID_PARAM;
    }

    /* 检查是否重复注册 */
    for (uint8_t i = 0; i < g_device_count; i++) {
        if (g_device_table[i].device_type == dev_if->device_type) {
            return ERR_ALREADY_INITIALIZED;
        }
    }

    /* 复制设备信息 */
    g_device_table[g_device_count] = *dev_if;

    /* 调用设备初始化 */
    if (dev_if->init != NULL) {
        int ret = dev_if->init();
        if (ret != ERR_OK) return ret;
    }

    g_device_count++;
    return ERR_OK;
}

/**
 * @brief   注销设备
 * @param   device_type: 设备类型标识
 * @return  ERR_OK 或错误码
 */
int protocol_unregister_device(uint8_t device_type)
{
    for (uint8_t i = 0; i < g_device_count; i++) {
        if (g_device_table[i].device_type == device_type) {
            /* 调用反初始化 */
            if (g_device_table[i].deinit != NULL) {
                g_device_table[i].deinit();
            }

            /* 移除设备 (将末尾设备移动到此位置) */
            g_device_count--;
            if (i < g_device_count) {
                g_device_table[i] = g_device_table[g_device_count];
            }
            return ERR_OK;
        }
    }
    return ERR_SENSOR_NOT_FOUND;
}

/**
 * @brief   查找设备
 * @param   device_type: 设备类型
 * @return  设备接口指针 (NULL=未找到)
 */
device_interface_t* device_find(uint8_t device_type)
{
    for (uint8_t i = 0; i < g_device_count; i++) {
        if (g_device_table[i].device_type == device_type) {
            return &g_device_table[i];
        }
    }
    return NULL;
}

/**
 * @brief   读取设备数据
 * @param   device_type: 设备类型
 * @param   data: 数据缓冲区
 * @param   len:  数据长度
 * @return  ERR_OK 或错误码
 */
int device_read(uint8_t device_type, void *data, uint16_t len)
{
    device_interface_t *dev = device_find(device_type);
    if (dev == NULL) return ERR_SENSOR_NOT_FOUND;
    if (dev->read == NULL) return ERR_INVALID_PARAM;

    return dev->read(data, len);
}

/**
 * @brief   写入设备数据
 */
int device_write(uint8_t device_type, void *data, uint16_t len)
{
    device_interface_t *dev = device_find(device_type);
    if (dev == NULL) return ERR_SENSOR_NOT_FOUND;
    if (dev->write == NULL) return ERR_INVALID_PARAM;

    return dev->write(data, len);
}

/**
 * @brief   扫描所有已注册设备
 * @return  在线设备数量
 */
uint8_t device_scan_all(void)
{
    uint8_t online_count = 0;

    for (uint8_t i = 0; i < g_device_count; i++) {
        device_interface_t *dev = &g_device_table[i];
        if (dev->init != NULL) {
            int ret = dev->init();
            if (ret == ERR_OK) {
                online_count++;
            }
        }
    }
    return online_count;
}

/**
 * @brief   获取已注册设备数量
 */
uint8_t device_get_count(void)
{
    return g_device_count;
}
