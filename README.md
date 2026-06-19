# SmartAgricultureOS

基于 OpenHarmony LiteOS 的智慧农业嵌入式控制系统

## 项目简介

基于 ARM Cortex-A 架构主控（适配 STM32/ESP32 双模硬件平台），依托 OpenHarmony LiteOS 轻量内核，搭建高实时性、多任务智慧农业嵌入式控制系统。

## 核心技术栈

- **内核**: OpenHarmony LiteOS
- **语言**: C
- **硬件平台**: STM32 / ESP32 (ARM Cortex-A)
- **外设通信**: I2C / SPI / UART / GPIO
- **多任务同步**: 信号量 / 消息队列
- **调试工具**: 示波器 / 逻辑分析仪
- **传感器**: 温湿度、土壤湿度、光照、CO₂浓度
- **执行设备**: 水泵、遮阳帘、通风风扇

## 项目结构

```
SmartAgricultureOS/
├── config/             # 系统配置文件
│   ├── system_config.h     # 系统参数配置
│   └── pin_config.h        # 引脚定义配置
├── include/            # 公共头文件
│   ├── common_types.h      # 通用类型定义
│   ├── error_codes.h       # 错误码定义
│   └── device_protocol.h   # 设备通信协议
├── src/
│   ├── drivers/        # 底层外设驱动
│   │   ├── gpio_drv.c      # GPIO 驱动
│   │   ├── i2c_drv.c       # I2C 驱动
│   │   ├── spi_drv.c       # SPI 驱动
│   │   └── uart_drv.c      # UART 驱动
│   ├── tasks/          # LiteOS 任务模块
│   │   ├── task_manager.c  # 任务管理器
│   │   ├── sensor_task.c   # 环境采集任务
│   │   ├── control_task.c  # 设备控制任务
│   │   ├── alarm_task.c    # 异常检测任务
│   │   ├── report_task.c   # 数据上报任务
│   │   └── command_task.c  # 指令解析任务
│   ├── business/       # 业务逻辑层
│   │   ├── sensor_hub.c    # 传感器数据采集中心
│   │   ├── actuator_mgr.c  # 执行设备管理器
│   │   ├── threshold_mgr.c # 阈值预警管理
│   │   └── cloud_proto.c   # 云端通信协议
│   └── utils/          # 工具模块
│       ├── data_filter.c   # 数据滤波算法
│       ├── crc_check.c     # CRC 校验
│       └── ring_buffer.c   # 环形缓冲区
├── docs/               # 文档
│   ├── driver_spec.md      # 驱动规格说明
│   └── task_design.md      # 任务设计方案
├── tests/              # 测试用例
│   └── unit_tests.c
├── tools/              # 辅助工具脚本
│   └── debug_scripts.md
├── Makefile            # 构建脚本
└── README.md
```

## 功能特性

- [x] 多传感器高精度数据采集（温湿度、土壤湿度、光照、CO₂）
- [x] 均值滤波与去极值算法，消除数据跳动误差
- [x] 多任务调度：采集、控制、报警、上报、指令解析独立运行
- [x] 信号量实现临界资源互斥访问
- [x] 消息队列完成跨任务数据传输
- [x] 多级阈值预警机制（本地蜂鸣器 + 云端推送）
- [x] 执行设备自动联动调控（水泵、遮阳帘、通风风扇）
- [x] 标准化设备接入接口，支持外设快速拓展
- [x] 外设通信稳定性 ≥ 99.8%
- [x] 系统响应延时 ≤ 10ms

## 构建与烧录

### STM32 平台
```bash
make PLATFORM=STM32
make flash
```

### ESP32 平台
```bash
make PLATFORM=ESP32
make flash
```

## 作者

王嘉晟
