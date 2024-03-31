#!/bin/bash

echo Selecting: $1
rm -f spheres.c
ln -s $1 spheres.c
