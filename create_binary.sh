#! /bin/sh
esptool --chip esp32s3 merge_bin   -o target_full.bin   0x0 build/bootloader/bootloader.bin   0x8000 build/partition_table/partition-table.bin   0xf000 build/ota_data_initial.bin   0x20000 build/minimal_watch_firmware.bin
