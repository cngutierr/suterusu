#!/bin/bash
printf "1k\t2k\t4k\t8k\t16k\t32k\t64k\t128k\t256k\t512k\t1m\t2m\t4m\t8m\t16m\t32m\t64m\n" > baseline_real.txt
for i in `seq 1 100`;
do
    ./new_process_lVs.sh baseline
done
