#!/bin/bash
#/main 500 40960000 4096 0
echo "running 4k"
./main 500 4096 4096 1 > 4k.txt
echo "running 512k"
./main 500 524288 4096 1 > 512k.txt
echo "running 32mb"
./main 500 33554432 4096 1 > 32mb.txt
