#!/bin/bash
printf "1k\t2k\t4k\t8k\t16k\t32k\t64k\t128k\t256k\t512k\t1m\t2m\t4m\t8m\t16m\t32m\t64m\n" > $1_sys.txt
printf "1k\t2k\t4k\t8k\t16k\t32k\t64k\t128k\t256k\t512k\t1m\t2m\t4m\t8m\t16m\t32m\t64m\n" > $1_user.txt
printf "1k\t2k\t4k\t8k\t16k\t32k\t64k\t128k\t256k\t512k\t1m\t2m\t4m\t8m\t16m\t32m\t64m\n" > $1_real.txt
for i in `seq 1 10`;
do
    rm /var/log/audit/audit.*
    ./process_lVs.sh decms
    sudo service auditd rotate
done
