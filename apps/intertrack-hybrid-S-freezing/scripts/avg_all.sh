#!/bin/bash

for case in freeze*
do
    echo Processing $case
    echo ------------------------------------
    ./sel $case
    ./avg.sh
done

rm test
