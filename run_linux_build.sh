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

QEMU_EXTRA_ARGS=(--qemu-extra-args "-chardev serial,id=ble_uart,path=/tmp/mw_qemu -serial chardev:ble_uart")

if [ "$1" == "debug" ]; then
    echo 'Run "xtensa-esp32s3-elf-gdb qemu_build/*.elf -ex \"target remote localhost:3333\""'

    idf.py -B qemu_build qemu \
        --flash-file qemu_build/qemu_flash.bin \
        --graphics \
        --gdb monitor
elif [ "$1" == "no_ble" ]; then
    idf.py -B qemu_build qemu \
        --flash-file qemu_build/qemu_flash.bin \
        --graphics \
        monitor
else
    idf.py -B qemu_build qemu \
        --flash-file qemu_build/qemu_flash.bin \
        --graphics \
        "${QEMU_EXTRA_ARGS[@]}" \
        monitor
fi
