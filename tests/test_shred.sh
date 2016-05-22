#!/bin/bash
echo "Rotating log file..."
service auditd rotate
echo "Delete old log files"
rm /var/log/audit/audit.*
echo "copying a large file to shred"
cp /home/krix/experiment/random_copy/original/000/000816.pdf .
echo "shreding a large file"
shred -n 1 000816.pdf
sleep 0.5
echo "checking the logs"
ausearch -m KERNEL_OTHER | tail -n 5
rm 000816.pdf
