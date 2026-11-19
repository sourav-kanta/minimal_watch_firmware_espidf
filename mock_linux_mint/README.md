# Emulating on Linux

## Building for Linux
1. Navigate to the project root directory
2. Export the esp idf dependencies
3. Run ```source .linux_build.sh```

## Running the emulated build
1. Make sure to build the project after every change as flash wont be properly stitched otherwise
2. Three use cases for running the build :
    1. Standalone run without phone app -> Run ```./run_linux_build.sh no_ble```
    2. Run with phone app ->
        1. Open up 3 terminals simultaneously
        2. In first terminal run ```socat -d -d pty,raw,echo=0,link=/tmp/mw_python pty,raw,echo=0,link=/tmp/mw_qemu```
        3. In second terminal, navigate to ```mock_linux_mint/ble_to_serial_tunnel```, then install the dependencies using ```pip install -r requirements.txt```. Once done, run the tunneling script, ```python tunnel.py```. Open up phone app, connect to MW Watch from the settings page. Wait for BLE notifications enabled message to appear
        4. Once notifications are enabled, In the third terminal, navigate to project directory and run ```./run_linux_build.sh```
    3. Debug build -> Open up two terminals and navgigate to project directory, on first terminal run ```./run_linux_build.sh debug``` and on the second terminal ```xtensa-esp32s3-elf-gdb qemu_build/*.elf -ex "target remote localhost:3333"```
3. Navigate using 'a' -> left, 'd' -> right,  ENTER and ESC keys
