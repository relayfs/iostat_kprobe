#!/bin/bash

while [ -n 1 ]
do
	if test -e /sys/kernel/dk_iostat/dk_iostat
	then
		date +'%D %T' >> iostats
		cat /sys/kernel/dk_iostat/dk_iostat >> iostats
		sleep 180
	else
		sleep 180
	fi
done
