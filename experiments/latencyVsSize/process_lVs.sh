#!/bin/bash
./lVs.sh 2> tmp

#echo "real time"
cat tmp | grep -v file | grep real | sed 's/\t0//' | tr -d 'realsm' | tr '\n' '\t' >> $1_real.txt
printf "\n" >> $1_real.txt

#echo "user time"
cat tmp | grep -v shredding | grep user | sed 's/\t0//' | tr -d 'userm' | tr '\n' '\t' >> $1_user.txt
printf "\n" >> $1_user.txt

#echo "sys time"
cat tmp | grep -v shredding | grep sys | sed 's/\t0//' | tr -d 'sym' | tr '\n' '\t' >> $1_sys.txt
printf "\n" >> $1_sys.txt
rm tmp
