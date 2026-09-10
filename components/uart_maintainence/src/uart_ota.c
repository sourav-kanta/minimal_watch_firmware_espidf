#include <uart_ota.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_log.h>
#include <stdatomic.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/uart.h>
#include <esp_rom_crc.h>
#include <string.h>
#include <esp_err.h>

static atomic_bool update_underway = false;
static esp_ota_handle_t update_handle = 0;
static const esp_partition_t *update_partition = NULL;
static const char* TAG = "Firmware Update";
static int retries = 0;

#define BAUD_RATE       115200
#define CMD_ACK         0x06
#define CMD_NACK        0x15
#define CMD_EOT         0x04
#define TX_BUFF_SIZE    (4*1024)
#define RX_BUFF_SIZE    (4*1024) 
#define BLOCK_SIZE_CRC  4
#define BLOCK_SIZE      ((2*1024) + BLOCK_SIZE_CRC)
#define MAX_RETRIES     3

void uart_ota_mark_firmware_valid(void) {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error cancelling rollback : %s", esp_err_to_name(err));
    } 
}

static void trigger_reboot(bool success) {
    esp_log_set_level_master(ESP_LOG_INFO);
    esp_log_level_set("*", ESP_LOG_INFO);
    if(!success) {
        ESP_LOGE(TAG, "Invalid app update received. skipping update");
    }
    esp_restart();
}

void uart_ota_start_firmware_update(void) {
    if(atomic_load(&update_underway)) return;
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find an inactive OTA partition");
        trigger_reboot(false);
    }
    ESP_LOGI(TAG, "Targeting partition subtype %d at offset 0x%" PRIx32, 
             update_partition->subtype, update_partition->address);

    // Turn off logging so it doesnt affect the UART
    ESP_LOGE(TAG, "Turning off logging so we can use pins for OTA");
    esp_log_level_set("*", ESP_LOG_NONE);
    esp_log_set_level_master(ESP_LOG_NONE);

    if (!uart_is_driver_installed(UART_NUM_0)) {
        uart_driver_install(UART_NUM_0, RX_BUFF_SIZE, TX_BUFF_SIZE, 0, NULL, 0);
    }

    uart_wait_tx_idle_polling(UART_NUM_0);
    uart_set_baudrate(UART_NUM_0, BAUD_RATE);
    uart_flush_input(UART_NUM_0);

    atomic_store(&update_underway, true);
    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK) {
        trigger_reboot(false);
    }

    // Start receiving bytes over OTA
    uint8_t *data_buffer = heap_caps_malloc(BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!data_buffer) trigger_reboot(false);

    int total_received = 0;
    uint32_t expected_app_size = 0;
    const uart_port_t uart_num = UART_NUM_0;

    int size_len = uart_read_bytes(uart_num, &expected_app_size, sizeof(expected_app_size), pdMS_TO_TICKS(15000));
    if (size_len != sizeof(expected_app_size) || expected_app_size == 0) {
        trigger_reboot(false);
    }

    uint8_t cmd = CMD_ACK;
    uart_write_bytes(uart_num, &cmd, sizeof(cmd));

    while (total_received < expected_app_size) {
        int remaining_app_bytes = expected_app_size - total_received;
        int block_data_size = BLOCK_SIZE - BLOCK_SIZE_CRC;
        int expected_payload = (remaining_app_bytes < block_data_size) ? remaining_app_bytes : block_data_size;
        int expected_uart_bytes = expected_payload + BLOCK_SIZE_CRC;

        int bytes_in_block = 0;
        bool block_success = true;
        int block_retries = 0;
        while (bytes_in_block < expected_uart_bytes) {
            int len = uart_read_bytes(uart_num,
                                      data_buffer + bytes_in_block,
                                      expected_uart_bytes - bytes_in_block,
                                      pdMS_TO_TICKS(2000)); 

            if (len < 0) trigger_reboot(false); 
            else if(len == 0) {
                block_success = false;
                block_retries++;
                if(block_retries >= MAX_RETRIES) {
                    break;
                }
            }

            bytes_in_block += len;
        }

        if(!block_success) {
            cmd = CMD_NACK;
            uart_flush_input(uart_num);
            uart_write_bytes(uart_num, &cmd, sizeof(cmd));
            retries += 1;
            if(retries >= MAX_RETRIES) {
                trigger_reboot(false);;
            }
            bytes_in_block = 0;
            continue; 
        }

        else if (bytes_in_block > BLOCK_SIZE_CRC) {

            uint32_t received_crc = 0;
            memcpy(&received_crc, &data_buffer[bytes_in_block - BLOCK_SIZE_CRC], BLOCK_SIZE_CRC*sizeof(uint8_t));
            uint32_t calc_crc = esp_rom_crc32_le(0, data_buffer, bytes_in_block - BLOCK_SIZE_CRC);
            bool checksum_ok = (calc_crc == received_crc); 


            if (checksum_ok) {
                retries = 0;
                esp_err_t write_err = esp_ota_write(update_handle, data_buffer, bytes_in_block - BLOCK_SIZE_CRC);
                if (write_err != ESP_OK) trigger_reboot(false);

                total_received += (bytes_in_block - BLOCK_SIZE_CRC);
                cmd = CMD_ACK;
                uart_write_bytes(uart_num, &cmd, sizeof(cmd));
            } else {
                cmd = CMD_NACK;
                uart_write_bytes(uart_num, &cmd, sizeof(cmd));
                retries += 1;
                if(retries >= MAX_RETRIES) {
                    trigger_reboot(false);
                } 
            }
        }
        else {
            cmd = CMD_NACK;
            uart_flush_input(uart_num);
            uart_write_bytes(uart_num, &cmd, sizeof(cmd));
            retries += 1;
            if(retries >= MAX_RETRIES) {
                trigger_reboot(false);
            }
        }

    }

    free(data_buffer);

    if(((uint32_t)total_received) != expected_app_size) {
        trigger_reboot(false);    
    }

    if (esp_ota_end(update_handle) == ESP_OK) {
        if (esp_ota_set_boot_partition(update_partition) == ESP_OK) {

            cmd = CMD_EOT;
            uart_write_bytes(uart_num, &cmd, sizeof(cmd));

            esp_log_set_level_master(ESP_LOG_INFO);
            esp_log_level_set("*", ESP_LOG_INFO);
            vTaskDelay(pdMS_TO_TICKS(200));
            trigger_reboot(true);
        }
    }

    trigger_reboot(false);
}

