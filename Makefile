PWD  := $(shell pwd)

# This is default values until you set them from env.
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-
KDIR ?= ../linux/

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
# ccflags-y += -DDEBUG

all: default

default:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	
MODULE_KO := $(MODULE_NAME).ko
DEVBOARD_LOCAL_IP := 10.42.0.194
DEVBOARD_USER := root
DEVBOARD_AUTH := $(DEVBOARD_USER)@$(DEVBOARD_LOCAL_IP)
DEVBOARD_USER_PASSWORD := root
DEVBOARD_MODPROBE_PATH := /lib/modules/3.0.35-2666-gbdde708/test
DEVBOARD_MODULE_PATH := $(DEVBOARD_MODPROBE_PATH)/$(MODULE_KO)

deploy_to_devboard: default
	sshpass -p $(DEVBOARD_USER_PASSWORD) scp -o StrictHostKeyChecking=no $(MODULE_KO) $(DEVBOARD_AUTH):$(DEVBOARD_MODULE_PATH)
	sshpass -p $(DEVBOARD_USER_PASSWORD) ssh -o StrictHostKeyChecking=no $(DEVBOARD_AUTH) "chmod a+x $(DEVBOARD_MODULE_PATH)"
	sshpass -p $(DEVBOARD_USER_PASSWORD) ssh -o StrictHostKeyChecking=no $(DEVBOARD_AUTH) "modprobe -r $(MODULE_NAME)"
	sshpass -p $(DEVBOARD_USER_PASSWORD) ssh -o StrictHostKeyChecking=no $(DEVBOARD_AUTH) "modprobe $(MODULE_NAME)"

