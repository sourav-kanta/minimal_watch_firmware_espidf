idf.py -B qemu_build -DEXTRA_COMPONENT_DIRS="mock_linux_mint" -DSDKCONFIG="sdkconfig.qemu" -DSDKCONFIG_DEFAULTS="sdkconfig;qemu_override.conf" build

echo "[+] Stitching qemu_flash.bin (4MB)..."
cd qemu_build || exit 1
esptool --chip esp32s3 merge-bin --pad-to-size 4MB -o qemu_flash.bin @flash_args
cd ..
echo "[+] Image updated successfully!"
