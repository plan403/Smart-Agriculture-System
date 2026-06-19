# 调试工具与脚本指南

## 1. 示波器调试

### 1.1 I2C 时序测量
```
连接:
  CH1 → SCL (PB6)
  CH2 → SDA (PB7)
  GND → GND

测量项:
  - SCL 频率: 应为 400KHz ± 5% (Fast Mode)
  - SDA 上升时间: < 300ns
  - SDA 建立时间: > 100ns (Fast Mode)
  - 起始条件: SDA↓ 时 SCL=H
  - 停止条件: SDA↑ 时 SCL=H
```

### 1.2 SPI 时序测量
```
连接:
  CH1 → SCK (PA5)
  CH2 → MOSI (PA7)
  CH3 → MISO (PA6)
  CH4 → CS (PA4)

测量项:
  - SCK 频率: 根据分频设置
  - CS→SCK 建立时间: > 50ns
  - SCK→CS 保持时间: > 50ns
  - 数据有效窗口: SCK边沿前后各 > 20ns
```

## 2. 逻辑分析仪

### 2.1 I2C 协议解析
```bash
# Saleae Logic / PulseView 配置
协议: I2C
SCL: Channel 0
SDA: Channel 1
地址显示: 7-bit

# 关注项
- 起始条件后第一个字节 (地址+R/W)
- 每个数据字节后的 ACK/NACK
- 重复起始条件时序
```

### 2.2 典型 I2C 通信波形
```
SHT30 温湿度读取:
START | 0x88(W) ACK | 0x2C ACK | 0x06 ACK | STOP
延时 20ms
START | 0x89(R) ACK | Data0 ACK | Data1 ACK | CRC ACK | ... | NACK | STOP
```

## 3. 串口调试

### 3.1 调试串口命令 (UART1, 115200 8N1)
```bash
# 连接
screen /dev/ttyUSB0 115200
# 或
minicom -D /dev/ttyUSB0 -b 115200

# 查看系统状态
AT+STATUS?     → 返回系统运行状态
AT+SENSOR?     → 返回最新传感器数据
AT+ALARM?      → 返回当前告警信息
AT+DEVICE?     → 返回设备状态
AT+THRESHOLD?  → 查看当前阈值
```

### 3.2 阈值设置命令
```bash
# 设置温度上限
AT+THRESHOLD=TEMP_HIGH,38.0
→ OK

# 设置土壤干旱阈值
AT+THRESHOLD=SOIL_DRY,20.0
→ OK

# 恢复默认
AT+THRESHOLD=DEFAULT
→ OK
```

## 4. 电源纹波测量

### 4.1 关键测试点
```
测试点          正常范围       异常表现
VCC_3.3V      3.3V ± 0.1V    I2C通信不稳定
VCC_5V        5.0V ± 0.25V   执行设备驱动力不足
VREF_ADC      3.3V ± 0.05V   传感器ADC值漂移
```

### 4.2 纹波测量
```
示波器设置:
  AC耦合, BW Limit=20MHz
  探头: x1, 接地弹簧

测量:
  3.3V 纹波: < 50mVpp (正常)
  > 100mVpp: 增加去耦电容 (100nF + 10uF)
```

## 5. 内存泄漏检测

### 5.1 栈使用监控
```c
// 在每个任务创建时记录栈基址
// 定期检查栈底 Magic Number
void mem_check_stack(void) {
    for (int i = 0; i < g_task_count; i++) {
        if (g_tasks[i].stack_base[0] != STACK_MAGIC) {
            LOG_ERROR("Task %d stack overflow!", i);
        }
    }
}
```

### 5.2 堆使用监控
```c
// 如果使用动态内存分配
static uint32_t g_malloc_total = 0;

void* safe_malloc(size_t size) {
    void *ptr = malloc(size + 4);
    if (ptr) {
        *((uint32_t*)ptr) = size;
        g_malloc_total += size;
        return (uint8_t*)ptr + 4;
    }
    return NULL;
}

void safe_free(void *ptr) {
    if (ptr) {
        uint32_t *size_ptr = (uint32_t*)((uint8_t*)ptr - 4);
        g_malloc_total -= *size_ptr;
        free(size_ptr);
    }
}
```

## 6. 性能分析

### 6.1 任务执行时间统计
```c
// 在任务入口和出口插入时间戳
uint32_t t_start = os_get_tick();
// ... task body ...
uint32_t t_elapsed = os_get_tick() - t_start;
if (t_elapsed > TASK_MAX_EXEC_TIME_MS) {
    LOG_WARN("Task %d exceeded time budget: %lu ms", task_id, t_elapsed);
}
```

### 6.2 中断延迟测量
```c
// GPIO引脚在ISR入口翻转，出口恢复
// 示波器测量高电平脉宽 = 中断延迟
void ISR_Handler(void) {
    PIN_SET_HIGH(DEBUG_PIN);
    // ... ISR body ...
    PIN_SET_LOW(DEBUG_PIN);
}
```

## 7. 长期稳定性测试

### 7.1 压力测试脚本
```bash
# 连续运行72小时监控
while true; do
    echo "=== $(date) ===" >> stability.log
    echo "AT+SENSOR?" > /dev/ttyUSB0
    sleep 10
done
```

### 7.2 故障注入测试
```
测试项:
  1. 传感器热拔插 → 检测自动恢复
  2. I2C总线短接 → 检测总线错误处理
  3. 电源波动 ±10% → 检测低压复位
  4. 电磁干扰 → 检测通信校验
```

## 8. 固件更新 (OTA)

### 8.1 OTA流程
```
1. 云端下发 OTA_START 指令
2. 设备进入OTA模式 (LED慢闪)
3. 分块接收固件数据
4. CRC32 整体校验
5. 写入Flash备用分区
6. 设置启动标志
7. 软件复位
8. Bootloader 验证并跳转新固件
```

### 8.2 回滚机制
```
- 保留上一个版本的固件
- 新固件启动失败3次 → 自动回滚
- 回滚标志存于EEPROM
```
