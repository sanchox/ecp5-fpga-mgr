PWD  := $(shell pwd)

# This is default values until you set them from env.
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-
KDIR ?= ../../imx-linux

MODULE_NAME = ecp5_sspi

ccflags-y += -Wno-strict-prototypes

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := main.o
$(MODULE_NAME)-objs += lattice/SSPIEm.o
$(MODULE_NAME)-objs += lattice/intrface.o
$(MODULE_NAME)-objs += lattice/core.o
$(MODULE_NAME)-objs += lattice/util.o
$(MODULE_NAME)-objs += lattice/hardware.o 
#$(MODULE_NAME)-objs += lattice/main.o


# Enable pr_debug() for all source code file
ccflags-y += -DDEBUG

all: default

default:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	
MODULE_KO := $(MODULE_NAME).ko
