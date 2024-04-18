#!/bin/bash

# Performs parallel speedup (strong scaling) study for different numbers of threads

RESULTS_DIR=OUTPUT
OUT_DIR=stats-CPU

mkdir -p "$RESULTS_DIR/$OUT_DIR"

for((i=1;i<=32;i++))
do
    if((i>9))
    then
	counter=$i
    else
	counter=0$i
    fi
    echo CPU Run No. $counter
    OMP_NUM_THREADS=$i ./spheres > "$RESULTS_DIR/$OUT_DIR/run$counter.log"
done
echo Done.
