#!/bin/bash

target=iostat_kprobe

if [ ! -z $(lsmod | grep $target | awk '{print $1}') ]
then
	echo "rmmod $target.ko"
	rmmod $target.ko
fi
