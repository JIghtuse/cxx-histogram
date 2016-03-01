#!/usr/bin/gnuplot

set grid
set terminal pngcairo size 1920,1080 lw 5
set output 'histogram.png'
set logscale x 2
set key font ",18"
set tics font ",9"
set xlabel "size of bitmap, pixels" font ",18"
set ylabel "running time, seconds" font ",18"
set title "Histogram calculation running time, nthreads=4 \n Intel(R) Core(TM) i3-2100 CPU @ 3.10GHz" font ",36"

plot 'results.txt' u 1:2 w lines title 'sequential', \
     'results.txt' u 1:3 w lines title 'transactional', \
     'results.txt' u 1:4 w lines title 'mutex', \
     'results.txt' u 1:5 w lines title 'atomic'

