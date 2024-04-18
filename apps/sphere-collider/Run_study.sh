#!/bin/bash

# Perfrorms 10 runs of ./spheres and evaluates eps_s after each run

RESULTS_DIR=OUTPUT
OUT_DIR=stats

mkdir -p "$RESULTS_DIR/$OUT_DIR"

for((i=1;i<=10;i++))
do
    if((i>9))
    then
	counter=$i
    else
	counter=0$i
    fi
    echo Run No. $counter
    ./spheres > "$RESULTS_DIR/$OUT_DIR/run$counter.log"
    cd "$RESULTS_DIR"
    ./calc_epss > "$OUT_DIR/eps_s$counter.txt"
    cd ..
done
echo Done.
