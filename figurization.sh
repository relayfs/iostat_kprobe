#!/bin/bash
obj_proc=iostat_delay
res_dir=result
image_dir=image
html_dir=html
res_image_dir=$res_dir/$image_dir
res_html_dir=$res_dir/$html_dir

if [ $# -ne 1 ]
then
	echo "Usage: command file"
	exit
fi

if [ -e $obj_proc ]
then
	./$obj_proc
else
	make $obj_proc
	if [ -ne $obj_proc ]
	then
		echo "ERROR:there is no such file ($obj_proc)"
		exit
	fi
fi

if [ ! -e $1 ]
then 
	echo "ERROR:there is no such file ($1)"
	exit
fi

if [ ! -d $res_dir ]
then
	mkdir $res_dir
	if [ ! -d $res_idr ]
	then 
		echo "ERROR:can not create such directory ($res_dir)"
		exit
	fi
fi

if [ ! -d $res_image_dir ]
then
	mkdir $res_image_dir
	if [ ! -d $res_image_idr ]
	then 
		echo "ERROR:can not create such directory ($res_image_dir)"
		exit
	fi
fi

if [ ! -d $res_html_dir ]
then
	mkdir $res_html_dir
	if [ ! -d $res_html_idr ]
	then 
		echo "ERROR:can not create such directory ($res_html_dir)"
		exit
	fi
fi

year=$(head -1 iostats | awk -F '[/ :]' '{print $3}')
month=$(head -1 iostats | awk -F '[/ :]' '{print $1}')
day=$(head -1 iostats | awk -F '[/ :]' '{print $2}')
hour=$(head -1 iostats | awk -F '[/ :]' '{print $4}')
minuter=$(head -1 iostats | awk -F '[/ :]' '{print $5}')
time_itv=3
let "acct_itv=60/$time_itv*24"
step=1
begin=1
let "over=$acct_itv+$begin"
loop_acct=0
xbegin=0
xover=$acct_itv
let "max_days=($(wc -l $1 | awk '{print $1}')-1)/$acct_itv+1"
format="l lw 2" 

echo "
<html>
<body>
" > total_result.html

while [ $loop_acct -ne $max_days ] 
do
var_time=$year-$month-$day

gnuplot << EOF
#!/bin/gnuplot
reset
set   term gif size 1280,512
set   title  "$1-$var_time-$hour-$minuter-iops"
set   output "$1-$var_time-iops.gif"
set   xlabel "time"
set   ylabel "iops"
set   ytics nomirror
set   xtics nomirror
unset autoscale y
unset autoscale x
set   xrange[$xbegin:$xover]
set   yrange[0.5:20000]
set   xtics 20
set   logscale y 2
set   ytics 2
set   grid x
set   grid y
plot  "$1" every $step::$begin::$over using 1:2 w $format t "rd/s",\
      "$1" every $step::$begin::$over using 1:3 w $format t "rdm/s",\
      "$1" every $step::$begin::$over using 1:5 w $format t "wt/s",\
      "$1" every $step::$begin::$over using 1:6 w $format t "wtm/s"
unset output

reset
set   term gif size 1280,512
set   title  "$1-$var_time-$hour-$minuter-thruput"
set   output "$1-$var_time-thruput.gif"
set   xlabel "time"
set   ylabel "thruput(kb/s)"
set   ytics nomirror
set   xtics nomirror
unset autoscale y
unset autoscale x
set   xrange[$xbegin:$xover]
set   yrange[0.5:204801]
set   xtics 20
set   logscale y 2
set   ytics 2
set   grid x
set   grid y
plot  "$1" every $step::$begin::$over using 1:4 w $format t "rtp/kbs",\
      "$1" every $step::$begin::$over using 1:7 w $format t "wtp/kbs"
unset output

reset
set   term gif size 1280,512
set   title  "$1-$var_time-$hour-$minuter-avsz"
set   output "$1-$var_time-avsz.gif"
set   ytics nomirror
set   xtics nomirror
unset autoscale y
unset autoscale x
set   xrange[$xbegin:$xover]
set   yrange[4:1100]
set   xtics 20
set   logscale y 2
set   ytics 2
set   xlabel "time"
set   ylabel "avsz(kb)"
set   grid x
set   grid y
plot  "$1" every $step::$begin::$over using 1:8 w $format t "rd_sz",\
      "$1" every $step::$begin::$over using 1:9 w $format t "wt_sz"
unset output
reset
set   term gif size 1280,512
set   title  "$1-$var_time-$hour-$minuter-iowait"
set   output "$1-$var_time-iowait.gif"
set   ytics nomirror
set   xtics nomirror
unset autoscale x
unset autoscale y
set   xrange[$xbegin:$xover]
set   yrange[0:401]
set   xtics 20
set   ytics 50
set   xlabel "time"
set   ylabel "iowait"
set   grid x
set   grid y
plot  "$1" every $step::$begin::$over using 1:11 w $format t "iowait"
unset output
reset
set   term gif size 1280,512
set   title  "$1-$var_time-$hour-$minuter-rate"
set   output "$1-$var_time-rate.gif"
set   ytics nomirror
set   xtics nomirror
unset autoscale y
unset autoscale x
set   xrange[$xbegin:$xover]
set   yrange[0:100]
set   xtics 20
set   ytics 10
set   xlabel "time"
set   ylabel "rate"
set   grid x
set   grid y
plot  "$1" every $step::$begin::$over using 1:10 w $format t "hrate",\
      "$1" every $step::$begin::$over using 1:13 w $format t "util"
unset output
reset
set   term gif size 1280,512
set   title  "$1-$var_time-$hour-$minuter-svctm"
set   output "$1-$var_time-svctm.gif"
set   ytics nomirror
set   xtics nomirror
unset autoscale x
unset autoscale y
set   xrange[$xbegin:$xover]
set   yrange[0:4]
set   xtics 20
set   ytics 0.5
set   xlabel "time"
set   ylabel "svctm"
set   grid x
set   grid y
plot  "$1" every $step::$begin::$over using 1:12 w $format t "svctm"
unset output
reset
EOF

echo "
<html>
<body>
<p>
<img src="../$image_dir/$1-$var_time-iops.gif" width="1280" height="512">
</p>
<p>
<img src="../$image_dir/$1-$var_time-thruput.gif" width="1280" height="512">
</p>
<p>
<img src="../$image_dir/$1-$var_time-avsz.gif" width="1280" height="512">
</p>
<p>
<img src="../$image_dir/$1-$var_time-iowait.gif" width="1280" height="512">
</p>
<p>
<img src="../$image_dir/$1-$var_time-svctm.gif" width="1280" height="512">
</p>
<p>
<img src="../$image_dir/$1-$var_time-rate.gif" width="1280" height="512">
</p>
</body>
</html>
" > $1-$var_time-result.html

mv *.gif $res_image_dir
mv $1-*.html $res_html_dir

echo "
<p>
<a href="./$html_dir/$1-$var_time-result.html"> $1-$var_time </a>
</p>
" >> total_result.html

let "loop_acct=$loop_acct+1"
let "begin=$begin+$acct_itv+1"
let "over=$begin+$acct_itv"
let "day=$day+1"
done

echo "
</body>
</html>
" >> total_result.html
mv total_result.html $res_dir

./clean.sh
