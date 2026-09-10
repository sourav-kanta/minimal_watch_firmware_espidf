import serial
import time
import sys
import os
import zlib
import struct
import socket

GREEN = '\033[92m'
YELLOW = '\033[93m'
RED = '\033[91m'
CYAN = '\033[96m'
RESET = '\033[0m'
BOLD = '\033[1m'

BAUD_RATE = 115200
PAYLOAD_SIZE = 2*1024

ACK = b'\x06'
NACK = b'\x15'
EOT = b'\x04'

def print_progress_bar(bytes_sent, total_size, length=40):
    percent = 100 * (bytes_sent / float(total_size))
    filled_length = int(length * bytes_sent // total_size)
    bar = '#' * filled_length + '-' * (length - filled_length)
    sys.stdout.write(f'\r{BOLD}{CYAN}Flashing:{RESET} [{GREEN}{bar}{RESET}] {percent:.1f}% ({bytes_sent}/{total_size} bytes)')
    sys.stdout.flush()

def upload_firmware(port, filename):
    if not os.path.exists(filename):
        print(f"{RED}{BOLD}Error:{RESET} File '{filename}' not found.")
        sys.exit(1)

    file_size = os.path.getsize(filename)
    
    try:
        ser = serial.serial_for_url(port, baudrate=BAUD_RATE, timeout=15)
        if hasattr(ser, '_socket'):
            print(f"{GREEN}{BOLD} Info : Detected QEMU, adding a NODELAY flag to socket{RESET}")
            ser._socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except serial.SerialException as e:
        print(f"{RED}{BOLD}Error opening serial port:{RESET} {e}")
        sys.exit(1)

    print(f"{BOLD}ESP32-S3 UART OTA (Size Handshake + CRC32){RESET}")
    print(f"Target Port: {CYAN}{port}{RESET}")
    print(f"Baud Rate:   {YELLOW}{BAUD_RATE}{RESET}")
    print(f"Payload:     {GREEN}{file_size}{RESET} bytes\n")
    
    time.sleep(1)

    print(f"{YELLOW}Negotiating file size with watch...{RESET}", end="", flush=True)
    size_bytes = struct.pack('<I', file_size)
    ser.write(size_bytes)
    ser.flush()
    
    size_ack = ser.read(1)
    if size_ack == ACK:
        print(f" {GREEN}ACK received.{RESET}\n")
    else:
        print(f" {RED}Failed! Expected ACK but got: {size_ack}. Aborting.{RESET}")
        ser.close()
        sys.exit(1)
    
    time.sleep(0.5)

    with open(filename, 'rb') as f:
        block_num = 1
        bytes_sent = 0
        
        print_progress_bar(bytes_sent, file_size)
        
        while True:
            chunk = f.read(PAYLOAD_SIZE)
            if not chunk:
                break
            
            chunk_crc = zlib.crc32(chunk) & 0xFFFFFFFF
            crc_bytes = struct.pack('<I', chunk_crc)
            payload = chunk + crc_bytes
            
            retry_count = 0
            while retry_count < 3:
                for i in range (0, len(payload), 128) :
                    ser.write(payload[i:i+128])
                    ser.flush()
                    time.sleep(0.015)
                #ser.write(payload)
                response = ser.read(1)
                
                if response == ACK:
                    bytes_sent += len(chunk)
                    print_progress_bar(bytes_sent, file_size)
                    break 
                elif response == NACK:
                    print(f"\n{YELLOW}NACK on block {block_num} (CRC Mismatch). Retrying...{RESET}")
                    retry_count += 1
                    print_progress_bar(bytes_sent, file_size)
                    ser.reset_input_buffer()
                    
            if retry_count >= 3:
                print(f"\n{RED}{BOLD}Fatal Error: Max retries reached. Aborting.{RESET}")
                ser.close()
                sys.exit(1)
            
            block_num += 1

    print(f"\n\n{GREEN}{BOLD}Done. All binary chunks transmitted.{RESET}")
    print(f"{YELLOW}Awaiting final flash validation and EOT from watch...{RESET}")
    
    final_resp = ser.read(1)
    if final_resp == EOT:
        print(f"{GREEN}{BOLD}OTA Update Complete! Watch is rebooting.{RESET}")
    else:
        print(f"{YELLOW}Update finished, but received unexpected final byte: {final_resp}{RESET}")

    ser.close()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"{RED}{BOLD}Usage:{RESET} python uploader.py <COM_PORT> <firmware.bin>")
        sys.exit(1)
        
    upload_firmware(sys.argv[1], sys.argv[2])
