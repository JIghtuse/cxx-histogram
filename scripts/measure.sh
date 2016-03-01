#!/bin/bash

rm -f results.txt
for i in $(seq 8 30); do
    pow=$(bc <<< 2^$i)
    ../build/hist --bitmap-size "$pow" --nthreads 4 >> results.txt
done
