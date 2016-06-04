#!/bin/bash
#/main 500 40960000 4096 0
echo "running 2mb"
./main 500 2097152 4096 1 > 2mb.txt
echo "running 4mb"
./main 500 4194304 4096 1 > 4mb.txt
echo "running 8mb"
./main 500 8388608 4096 1 > 8mb.txt
echo "running 16mb"
./main 500 16777216 4096 1 > 16mb.txt
echo "running 32mb"
./main 500 33554432 4096 1 > 32mb.txt
echo "running 64mb"
./main 500 67108864 4096 1 > 64mb.txt

