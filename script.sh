#!/bin/bash

while test -e /sys/kernel/dk_iostat/dk_iostat
do
	date >> iostats
	cat /sys/kernel/dk_iostat/dk_iostat >> iostats
	sleep 60
done
