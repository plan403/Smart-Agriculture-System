# ================================================================
# SmartAgricultureOS Makefile
# 基于 OpenHarmony LiteOS 的智慧农业嵌入式控制系统
# 适配 STM32 / ESP32 双平台
# ================================================================

# ---- 工具链配置 ----
ifeq ($(PLATFORM), ESP32)
    CROSS_COMPILE = xtensa-esp32-elf-
    MCU_FLAGS     = -mcpu=xtensa-lx6
    LD_SCRIPT     = esp32.ld
    DEFINES       = -DTARGET_PLATFORM=2
else
    CROSS_COMPILE = arm-none-eabi-
    MCU_FLAGS     = -mcpu=cortex-m3 -mthumb
    LD_SCRIPT     = stm32f103.ld
    DEFINES       = -DTARGET_PLATFORM=1
endif

CC      = $(CROSS_COMPILE)gcc
AS      = $(CROSS_COMPILE)as
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
SIZE    = $(CROSS_COMPILE)size

# ---- 目录结构 ----
SRC_DIR     = src
DRV_DIR     = src/drivers
TASK_DIR    = src/tasks
BIZ_DIR     = src/business
UTIL_DIR    = src/utils
INC_DIR     = include
CFG_DIR     = config
BUILD_DIR   = build

# ---- 源文件 ----
SRC_C = $(SRC_DIR)/main.c \
        $(DRV_DIR)/gpio_drv.c \
        $(DRV_DIR)/i2c_drv.c \
        $(DRV_DIR)/spi_drv.c \
        $(DRV_DIR)/uart_drv.c \
        $(TASK_DIR)/task_manager.c \
        $(TASK_DIR)/sensor_task.c \
        $(TASK_DIR)/control_task.c \
        $(TASK_DIR)/alarm_task.c \
        $(TASK_DIR)/report_task.c \
        $(TASK_DIR)/command_task.c \
        $(BIZ_DIR)/sensor_hub.c \
        $(BIZ_DIR)/actuator_mgr.c \
        $(BIZ_DIR)/threshold_mgr.c \
        $(BIZ_DIR)/cloud_proto.c \
        $(UTIL_DIR)/data_filter.c \
        $(UTIL_DIR)/crc_check.c \
        $(UTIL_DIR)/ring_buffer.c \
        $(UTIL_DIR)/error_codes.c

# ---- 目标文件 ----
OBJ = $(SRC_C:%.c=$(BUILD_DIR)/%.o)
TARGET = $(BUILD_DIR)/SmartAgricultureOS

# ---- 编译选项 ----
INCLUDES = -I$(INC_DIR) -I$(CFG_DIR)

CFLAGS  = $(MCU_FLAGS)
CFLAGS += -O2                          # 优化等级
CFLAGS += -g3                          # 调试信息
CFLAGS += -Wall -Wextra                # 警告全开
CFLAGS += -Wno-unused-parameter
CFLAGS += -ffunction-sections          # 未用函数剔除
CFLAGS += -fdata-sections              # 未用数据剔除
CFLAGS += -fno-common
CFLAGS += -std=c11
CFLAGS += $(DEFINES)
CFLAGS += $(INCLUDES)

LDFLAGS  = $(MCU_FLAGS)
LDFLAGS += -T$(LD_SCRIPT)
LDFLAGS += -Wl,--gc-sections           # 链接时剔除未用段
LDFLAGS += -Wl,-Map=$(TARGET).map
LDFLAGS += -nostartfiles
LDFLAGS += -lm                         # 数学库
LDFLAGS += -lc
LDFLAGS += -lnosys

# ---- 构建规则 ----
.PHONY: all clean flash size debug

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex size

# 编译
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接
$(TARGET).elf: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

# 生成 bin 文件
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# 生成 hex 文件
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# 反汇编
$(TARGET).lst: $(TARGET).elf
	$(OBJDUMP) -d $< > $@

# 显示尺寸信息
size: $(TARGET).elf
	@echo "============================================"
	@echo "  SmartAgricultureOS 固件尺寸"
	@echo "============================================"
	$(SIZE) $<

# ---- 烧录 ----
flash: $(TARGET).bin
ifeq ($(PLATFORM), ESP32)
	esptool.py --chip esp32 write_flash 0x10000 $<
else
	st-flash write $< 0x08000000
endif

# ---- 调试 ----
debug: $(TARGET).elf
ifeq ($(PLATFORM), ESP32)
	xtensa-esp32-elf-gdb $< -ex "target remote :3333"
else
	arm-none-eabi-gdb $< -ex "target extended-remote :3333"
endif

# ---- 清理 ----
clean:
	rm -rf $(BUILD_DIR)

# ---- 帮助 ----
help:
	@echo "SmartAgricultureOS 构建系统"
	@echo ""
	@echo "使用方法:"
	@echo "  make PLATFORM=STM32          - 编译 STM32 版本"
	@echo "  make PLATFORM=ESP32          - 编译 ESP32 版本"
	@echo "  make flash PLATFORM=STM32    - 编译并烧录"
	@echo "  make debug PLATFORM=STM32    - 启动GDB调试"
	@echo "  make clean                   - 清理构建文件"
	@echo "  make size                    - 查看固件尺寸"
	@echo "  make help                    - 显示帮助信息"
