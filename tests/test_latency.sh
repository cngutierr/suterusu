#!/bin/bash
echo "testing touch with no extra arguments:"
time {
    for i in `seq 1 10000`;
    do
        touch latencytest.file
    done
}
echo "testing touch and setting a past date:"
time {
    for i in `seq 1 10000`;
    do
        touch -t 201209141013 latencytest.file
    done
}

