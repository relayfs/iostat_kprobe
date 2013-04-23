#!/bin/bash

while test -e /sys/kernel/dk_iostat/dk_iostat
do
	date +'%D %T' >> iostats
	cat /sys/kernel/dk_iostat/dk_iostat >> iostats
	sleep 180
done
