#
# Configuration Options for KLFER application
#

# Build target
# 0: Native compile (x86_64)
# 1: Cross compile  (ARM64)
CONFIG_TARGET ?= 0

## set [y(=yes) | n(=no)]
# DEBUG mode
CONFIG_DEBUG ?= y

### Don't change the following ###

CFLAGS = -Wall -O2
LDFLAGS =

ifeq ($(CONFIG_TARGET), 0)
    CC = gcc
endif
ifeq ($(CONFIG_TARGET), 1)
    CC = aarch64-linux-gnu-gcc
endif

TARGET = klferctl

TOPDIR = ..
INCLUDE = -I$(TOPDIR)/include
SRC = klfer_app.c
OBJ = $(SRC:%.c=%.o)

ifeq ($(CONFIG_DEBUG), y)
    CFLAGS += -DDEBUG
endif

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) $(INCLUDE) -c $(SRC)

all: clean $(TARGET)

clean:
	rm -f $(TARGET) $(OBJ)

