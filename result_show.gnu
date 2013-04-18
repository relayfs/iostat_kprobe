
reset
set   term gif size 1280,512
set   output "request_count.gif"
set   ytics nomirror
set   xtics nomirror
unset autoscale y
set   yrange[1:20000]
set   logscale y 2
set   ytics 2
set   grid x
set   grid y
plot  "sda" using 1:2 w l lw 2 t "rd/s",\
      "sda" using 1:4 w l lw 2 t "rdm/s",\
      "sda" using 1:7 w l lw 2 t "wt/s",\
      "sda" using 1:9 w l lw 2 t "wtm/s"
unset output
reset

set   term gif size 1280,512
set   output "request_size.gif"
set   ytics nomirror
set   xtics nomirror
unset autoscale y
set   yrange[1:500]
set   logscale y 2
set   ytics 2
set   grid x
set   grid y
plot  "sda" using 1:12 w l lw 2 t "rd_sz",\
      "sda" using 1:13 w l lw 2 t "wt_sz"
unset output
reset

set   term gif size 1280,512
set   output "request_iowait.gif"
set   ytics nomirror
set   xtics nomirror
set   grid x
set   grid y
plot  "sda" using 1:15 w l lw 2 t "iowait"
unset output
reset

set   term gif size 1280,512
set   output "request_rate.gif"
set   ytics nomirror
set   xtics nomirror
unset autoscale y
set   yrange[0:100]
set   ytics 10
set   grid x
set   grid y
plot  "sda" using 1:14 w l lw 2 t "hrate",\
      "sda" using 1:17 w l lw 2 t "util"
unset output
reset 

set   term gif size 1280,512
set   output "request_svctm.gif"
set   ytics nomirror
set   xtics nomirror
set   grid x
set   grid y
plot  "sda" using 1:16 w l lw 2 t "svctm"
unset output
reset
