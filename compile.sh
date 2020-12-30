#!/bin/sh

# This compiles the C script using the bcm2835 Library
gcc -o show_flakes_relay -l rt show_flakes_relay.c -l bcm2835 -lm
