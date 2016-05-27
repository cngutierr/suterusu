#!/bin/bash
./lVs.sh | tr '\n' '\t' >> $1_real.txt
printf "\n" >> $1_real.txt

