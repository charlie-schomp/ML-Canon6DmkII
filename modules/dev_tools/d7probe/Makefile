# define the module name - make sure name is max 8 characters
MODULE_NAME = d7probe
MODULE_OBJS = $(BUILD_DIR)/d7probe.o \
              $(BUILD_DIR)/hooks_6d2.o
TOP_DIR = ../../..

# include modules environment
include $(TOP_DIR)/modules/Makefile
CFLAGS_ARCHLESS = $(filter-out -march=armv7-r -march=armv7-a -march=armv5te,$(CFLAGS))
.DEFAULT_GOAL := $(BUILD_DIR)/module_complete

# The hook entry stubs must be Thumb (the MMU hook jumps to an odd address and
# the replayed Canon instructions are Thumb-2); the rest is plain module code.
$(BUILD_DIR)/hooks_6d2.o: hooks_6d2.c
	$(CC) $(CFLAGS_ARCHLESS) -mthumb -march=armv7 $(CDEPS_FLAGS) -c -o $@ $<
