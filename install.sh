#!/bin/bash

path=/boot/
pre=System.map-
version=$(uname -r)
target=$path$pre$version

token_a=block_class
token_b=disk_type
token_c=__do_page_cache_readahead
token_d=elv_bio_merged
token_e=blk_finish_request
token_f=elv_merge_requests
token_g=get_request_wait

if test -e $target
then
	p_token_a=$(grep -w $token_a $target | awk '{print $1}')
	p_token_b=$(grep -w $token_b $target | awk '{print $1}')
	p_token_c=$(grep -w $token_c $target | awk '{print $1}')
	p_token_d=$(grep -w $token_d $target | awk '{print $1}')
	p_token_e=$(grep -w $token_e $target | awk '{print $1}')
	p_token_f=$(grep -w $token_f $target | awk '{print $1}')
	p_token_g=$(grep -w $token_g $target | awk '{print $1}')
fi

module=iostat_kprobe
token_a=p_$token_a
token_b=p_$token_b
token_c=p_$token_c
token_d=p_$token_d
token_e=p_$token_e
token_f=p_$token_f
token_g=p_$token_g

if [ ! -z $(lsmod | grep $target | awk '{print $1}') ]
then
	rmmod $module.ko
fi

insmod $module.ko $token_a=0x$p_token_a \
		  $token_b=0x$p_token_b \
		  $token_c=0x$p_token_c \
		  $token_d=0x$p_token_d \
		  $token_e=0x$p_token_e \
		  $token_f=0x$p_token_f \
		  $token_g=0x$p_token_g 

