#
# Configuration Options for KLFER kernel module
#

# Build target
# 0 : Native compile (x86_64)
# 1 : Cross compile  (ARM64)
CONFIG_TARGET ?= 0

## set [y(=yes) | n(=no)]
# DEBUG mode
CONFIG_DEBUG ?= y

### Don't change the following ###

SHARED_INCLUDE_DIR ?= $(PWD)/../include
ccflags-y += -I$(SHARED_INCLUDE_DIR)

ifeq ($(CONFIG_TARGET), 0)
# build for native compile
    KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
    ARCH ?= x86_64
    CROSS_COMPILE ?=
endif

ifeq ($(CONFIG_TARGET), 1)
# build for ARM64
    KERNEL_SRC ?= /mnt/hdd3/nt12/workspace/Petalinux-build/RELEASE/build/tmp/work/versal_generic-xilinx-linux/linux-xlnx/5.4+gitAUTOINC+22b71b4162-r0/linux-versal_generic-standard-build
    ARCH ?= arm64
    CROSS_COMPILE ?= aarch64-linux-gnu-
endif

MODULE_NAME = klfer
MODULE_OBJS = klfer_mod.o
ifeq ($(CONFIG_DEBUG), y)
    ccflags-y += -DDEBUG
	MODULE_OBJS += klfer_dbg.o
endif
$(MODULE_NAME)-y = $(MODULE_OBJS)

SRC=$(shell pwd)

CONFIG_KLFERMOD ?= m
obj-$(CONFIG_KLFERMOD) += $(MODULE_NAME).o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CONFIG_KLFERMOD=$(CONFIG_KLFERMOD) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean

