#!/bin/bash

(
    while true; do
        ID=$(wmctrl -l | awk '$NF=="QEMU" {print $1}')
        if [ -n "$ID" ]; then
            wmctrl -i -r "$ID" -b add,above,sticky
            break
        fi
        sleep 1
    done
) &

idf.py -B qemu_build qemu --graphics monitor
