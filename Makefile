# Makefile 2.6

#define_config
#----------iostat_kprobe.c-------------
#__PROC_INFO_ACCT__
#__PART0_INFO_TEST__
#__SHOW__
#-------------------------------------

obj-m := iostat_kprobe.o

KDIR := /lib/modules/$(shell uname -r)/build/
PWD  := $(shell pwd)
CC =  gcc -g -D __SHOW__ -D __PROC_INFO_ACCT__

all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean && rm -rf Module.*

