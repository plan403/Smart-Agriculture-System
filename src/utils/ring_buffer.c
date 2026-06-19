/**
 * @file    ring_buffer.c
 * @brief   环形缓冲区实现
 * @note    用于串口收发缓冲、数据暂存等场景
 *          无锁设计，适合单生产者单消费者模型
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ================================================================
 * 环形缓冲区结构
 * ================================================================ */
typedef struct {
    uint8_t *buffer;        /* 缓冲区内存 */
    uint16_t size;          /* 缓冲区总大小 */
    uint16_t head;          /* 读指针 */
    uint16_t tail;          /* 写指针 */
    uint16_t count;         /* 当前数据量 */
} ring_buffer_t;

/* ================================================================
 * 初始化与销毁
 * ================================================================ */

/**
 * @brief   初始化环形缓冲区
 * @param   rb:   缓冲区对象
 * @param   buf:  内存区域
 * @param   size: 缓冲区大小 (必须是2的幂)
 */
void ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint16_t size)
{
    rb->buffer = buf;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

/**
 * @brief   清空缓冲区
 */
void ring_buffer_clear(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

/* ================================================================
 * 数据写入
 * ================================================================ */

/**
 * @brief   写入单个字节
 * @return  true=成功, false=缓冲区满
 */
bool ring_buffer_put(ring_buffer_t *rb, uint8_t data)
{
    if (rb->count >= rb->size) {
        return false;  /* 缓冲区满 */
    }

    rb->buffer[rb->tail] = data;
    rb->tail = (rb->tail + 1) % rb->size;
    rb->count++;
    return true;
}

/**
 * @brief   写入多个字节
 * @return  实际写入的字节数
 */
uint16_t ring_buffer_write(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t written = 0;

    while (written < len && rb->count < rb->size) {
        rb->buffer[rb->tail] = data[written];
        rb->tail = (rb->tail + 1) % rb->size;
        rb->count++;
        written++;
    }
    return written;
}

/**
 * @brief   强制写入 (覆盖旧数据)
 */
void ring_buffer_put_force(ring_buffer_t *rb, uint8_t data)
{
    rb->buffer[rb->tail] = data;
    rb->tail = (rb->tail + 1) % rb->size;

    if (rb->count >= rb->size) {
        /* 覆盖旧数据，head前移 */
        rb->head = (rb->head + 1) % rb->size;
    } else {
        rb->count++;
    }
}

/* ================================================================
 * 数据读取
 * ================================================================ */

/**
 * @brief   读取单个字节
 * @param   data: 输出数据指针
 * @return  true=成功, false=缓冲区空
 */
bool ring_buffer_get(ring_buffer_t *rb, uint8_t *data)
{
    if (rb->count == 0) {
        return false;  /* 缓冲区空 */
    }

    *data = rb->buffer[rb->head];
    rb->head = (rb->head + 1) % rb->size;
    rb->count--;
    return true;
}

/**
 * @brief   读取多个字节
 * @return  实际读取的字节数
 */
uint16_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t read_count = 0;

    while (read_count < len && rb->count > 0) {
        data[read_count] = rb->buffer[rb->head];
        rb->head = (rb->head + 1) % rb->size;
        rb->count--;
        read_count++;
    }
    return read_count;
}

/**
 * @brief   查看单个字节 (不移除)
 * @param   data: 输出数据指针
 * @return  true=成功, false=缓冲区空
 */
bool ring_buffer_peek(ring_buffer_t *rb, uint8_t *data)
{
    if (rb->count == 0) {
        return false;
    }

    *data = rb->buffer[rb->head];
    return true;
}

/**
 * @brief   查看指定偏移的字节 (不移除)
 */
bool ring_buffer_peek_at(ring_buffer_t *rb, uint16_t offset, uint8_t *data)
{
    if (offset >= rb->count) {
        return false;
    }

    uint16_t index = (rb->head + offset) % rb->size;
    *data = rb->buffer[index];
    return true;
}

/* ================================================================
 * 状态查询
 * ================================================================ */

uint16_t ring_buffer_count(ring_buffer_t *rb)
{
    return rb->count;
}

uint16_t ring_buffer_free(ring_buffer_t *rb)
{
    return rb->size - rb->count;
}

bool ring_buffer_is_empty(ring_buffer_t *rb)
{
    return (rb->count == 0);
}

bool ring_buffer_is_full(ring_buffer_t *rb)
{
    return (rb->count >= rb->size);
}
