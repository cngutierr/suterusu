#!/bin/bash
#/main 500 40960000 4096 0
echo "running 4k"
./main 500 4096 4096 0 > 4096.txt
echo "running 8k"
./main 500 8192 4096 0 > 8192.txt
echo "running 16k"
./main 500 16384 4096 0 > 16384.txt
echo "running 32k"
./main 500 32768 4096 0 > 32768.txt
echo "running 65k"
./main 500 65536 4096 0 > 65536.txt
echo "running 128k"
./main 500 131072 4096 0 > 131072.txt
echo "running 256k"
./main 500 262144 4096 0 > 262144.txt
echo "running 512k"
./main 500 524288 4096 0 > 524288.txt
echo "running 1m"
./main 500 1048576 4096 0 > 1048576.txt

