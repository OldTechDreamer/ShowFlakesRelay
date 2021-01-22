#!/bin/sh

# This compiles the C script using the bcm2835 Library
gcc -o $2 -l rt $1 -l bcm2835 -lm
