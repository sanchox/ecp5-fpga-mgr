PWD  := $(shell pwd)

# This is default values until you set them from env.
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-
#CROSS_COMPILE ?= /opt/freescale/usr/local/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-none-linux-gnueabi-
KDIR ?= /home/sanchox/linux-3.0.35/

MODULE_NAME = ecp5_sspi

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
	
insmod_ko_on_kondor_ax: default
	sshpass -p "root" scp -o StrictHostKeyChecking=no ecp5_sspi.ko root@192.168.222.192:/lib/modules/3.0.35-2666-gbdde708/test/ecp5_sspi.ko
	sshpass -p "root" ssh -o StrictHostKeyChecking=no root@192.168.222.192 "chmod a+x /lib/modules/3.0.35-2666-gbdde708/test/ecp5_sspi.ko"
	sshpass -p "root" ssh -o StrictHostKeyChecking=no root@192.168.222.192 "modprobe -r ecp5_sspi"
	sshpass -p "root" ssh -o StrictHostKeyChecking=no root@192.168.222.192 "modprobe ecp5_sspi"

