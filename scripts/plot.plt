#!/usr/bin/gnuplot

set terminal pngcairo size 3840,2160 lw 10
set output 'histogram.png'
set logscale x 2
set key font ",36"
set tics font ",18"
set xlabel "size of bitmap, pixels" font ",36"
set ylabel "running time, seconds" font ",36"
set title "Histogram calculation running time, nthreads=8" font ",72"

plot 'results.txt' u 1:2 w lines title 'sequential', \
     'results.txt' u 1:3 w lines title 'transactional', \
     'results.txt' u 1:4 w lines title 'mutex', \
     'results.txt' u 1:5 w lines title 'atomic'

