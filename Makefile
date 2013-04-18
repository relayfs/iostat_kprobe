# Makefile 2.6

obj-m := iostat_kprobe.o

KDIR := /lib/modules/$(shell uname -r)/build/
PWD  := $(shell pwd)
CC =  gcc -g -D __DIRECT_SHOW__ 

all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean && rm -rf Module.*
