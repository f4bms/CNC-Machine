# Kbuild Makefile for building the kernel module
# Usage:
#   make
#   make clean

obj-m += driver.o
obj-m += driver_usage.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
