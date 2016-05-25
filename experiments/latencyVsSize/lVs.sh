#!/bin/bash
head -c 1k < /dev/zero > example
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 2k < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 4k < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example


head -c 8k < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example


head -c 16k < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example


head -c 32k < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example


head -c 64k < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example


head -c 128k < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 256k < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 512k < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 1m < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 2m < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 4m < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 8m < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 16m < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 32m < /dev/zero > example
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

head -c 64m < /dev/zero > example
 
st=`date +%s.%N`
shred -n 1 example 
et=`date +%s.%N`
echo $(python -c "print(${et} -${st})")
rm example

#head -c 128m < /dev/zero > example
# 
#rm example


