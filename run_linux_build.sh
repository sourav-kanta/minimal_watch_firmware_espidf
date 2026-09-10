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

if [ "$1" == "debug" ]; then
    echo 'Run "xtensa-esp32s3-elf-gdb qemu_build/*.elf -ex "target remote localhost:3333""'
    idf.py -B qemu_build qemu --flash-file qemu_build/qemu_flash.bin --graphics --gdb monitor
else
    idf.py -B qemu_build qemu --flash-file qemu_build/qemu_flash.bin --graphics monitor
fi
