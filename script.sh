#!/bin/bash

while test -e /sys/kernel/dk_iostat/dk_iostat
do
	date >> log
	cat /sys/kernel/dk_iostat/dk_iostat >> log
	sleep 180
done
