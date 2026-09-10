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

echo "[+] Starting QEMU standalone..."
qemu-system-xtensa -M esp32s3 -m 32M \
    -drive file=qemu_build/qemu_flash.bin,if=mtd,format=raw \
    -drive file=qemu_build/qemu_efuse.bin,if=none,format=raw,id=efuse \
    -global driver=nvram.esp32s3.efuse,property=drive,value=efuse \
    -global driver=timer.esp32s3.timg,property=wdt_disable,value=true \
    -nic user,model=open_eth -display sdl -serial tcp::5555,server,nowait &

QEMU_PID=$!
sleep 1

echo "[+] Connecting IDF Monitor to QEMU..."
idf.py -B qemu_build monitor -p socket://localhost:5555

echo -e "\n---------------------------------------------------"
echo "[+] Monitor detached! QEMU is still running (PID: $QEMU_PID)."
echo "[+] The 5555 socket is now free. Run flasher in another terminal:"
echo "    python ota_flasher.py socket://localhost:5555 qemu_build/minimal_watch_firmware.bin"
echo "---------------------------------------------------"

wait $QEMU_PID
