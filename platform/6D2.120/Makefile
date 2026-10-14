MODEL = 6D2
FW_VERSION = 120

TOP_DIR = ../..

AUTOEXEC_BASE = 0x800000
FIR_BASE = 0x800120

#CANON_FIR  = 6D200111.FIR
FIRMWARE_ID     = 0x80000406
#UPDATE_NAME_FIR = BLNK_6D2.FIR
MAIN_FIRMWARE_ADDR = 0xE0040000

# RESTARTSTART is the location in mem that ML binary will be copied to.
# This should be user_mem_start from cstart struct, aligned up to 0x10.
# 1.2.0: user_mem_start moved by 0x40 vs 1.1.1 (was 0xe0f90).  ML's boot code
# prints the value it wants if this is wrong: build with CONFIG_QEMU=y and look
# for "[BOOT] Wrong or unaligned RESTARTSTART address!".
RESTARTSTART = 0xe0fd0

# Cortex A9, binaries loaded as Thumb
PLATFORM_ARCH = armv7-a
CFLAGS_PLATFORM += -mthumb -mlong-calls

ML_BOOT_OBJ = boot-d678.o
ML_SRC_EXTRA_OBJS += $(BUILD_DIR)/function_overrides.o

include ../Makefile
.DEFAULT_GOAL := $(BUILD_DIR)/cam_complete
