#!/bin/bash

target=iostat_kprobe

if( $(lsmod | grep $target | xargs -0 test -z) )
then
	echo
else
	rmmod $target
fi
