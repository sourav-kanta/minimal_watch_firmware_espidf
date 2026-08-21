#include <imu_driver_private.h>
#include <imu_register_defs.h>
#include <imu_driver_HAL.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <gpio_pins.h>
#include <esp_memory_utils.h>

#define MAX_SINGLE_REG_DATA_LEN 3

static spi_device_handle_t spi_device = NULL;
static const char* TAG = "ESP32_IMU_Driver";

static imu_err_t esp32_imu_reg_read(void* intf_ptr, uint8_t reg_addr, uint8_t *data, size_t len) {
    if(spi_device == NULL) return IMU_FAILED;
    if(len > MAX_SINGLE_REG_DATA_LEN || !data) return IMU_INVALID_CONFIG;
    spi_device_handle_t spi = (spi_device_handle_t)intf_ptr;
    assert(spi);
    spi_transaction_t transaction = {
        .length = (1+len) * sizeof(uint8_t) * 8,
        .rxlength = (1+len) *  sizeof(uint8_t) * 8,
        .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA
    };

    memset(transaction.tx_data, 0, 8);
    transaction.tx_data[0] = reg_addr | 0x80;

    esp_err_t ret = spi_device_transmit(spi, &transaction);
    ESP_LOGD(TAG, "Read RX: %02X %02X %02X %02X",
        transaction.rx_data[0],
        transaction.rx_data[1],
        transaction.rx_data[2],
        transaction.rx_data[3]);
    ESP_LOGD(TAG, "Read TX: %02X %02X %02X %02X",
        transaction.tx_data[0],
        transaction.tx_data[1],
        transaction.tx_data[2],
        transaction.tx_data[3]);

    if(ret == ESP_OK) {
        memcpy(data, &transaction.rx_data[1], len);
        return IMU_OK;
    }
    return IMU_FAILED;
}

static imu_err_t esp32_imu_reg_write(void* intf_ptr, uint8_t reg_addr, const uint8_t *data, size_t len) {
    if(spi_device == NULL) return IMU_FAILED;
    if(len > MAX_SINGLE_REG_DATA_LEN || !data) return IMU_INVALID_CONFIG;
    spi_device_handle_t spi = (spi_device_handle_t)intf_ptr;
    assert(spi);
    spi_transaction_t transaction = {
        .length = (1+len) * sizeof(uint8_t) * 8,
        .flags = SPI_TRANS_USE_TXDATA
    };

    memset(transaction.tx_data, 0, 8);
    transaction.tx_data[0] = reg_addr & (0x7F);
    memcpy(&transaction.tx_data[1], data, len);

    ESP_LOGD(TAG, "Write TX: %02X %02X %02X %02X",
     transaction.tx_data[0],
     transaction.tx_data[1],
     transaction.tx_data[2],
     transaction.tx_data[3]);
    return (spi_device_transmit(spi, &transaction) == ESP_OK) ? IMU_OK : IMU_FAILED;
}

static imu_err_t esp32_imu_fifo_read(void* intf_ptr, uint8_t reg_addr, uint8_t *data, size_t len) {
    if (spi_device == NULL) return IMU_FAILED;
    if (!data || len == 0) return IMU_INVALID_CONFIG;
    spi_device_handle_t spi = (spi_device_handle_t)intf_ptr;
    assert(spi);
    if(!(esp_ptr_dma_capable(data) && (((uintptr_t)data % 4) == 0))) {
        ESP_LOGE(TAG, "Provided buffer is not DMA capable");
        return IMU_FAILED;
    }
    spi_transaction_t transaction = {
        .length = (1+len) * 8,
        .rxlength = (1+len) * 8,       
        .rx_buffer = data,
        .tx_buffer = data,
    };
    data[0] = reg_addr | 0x80;

    esp_err_t ret = spi_device_transmit(spi, &transaction);
    if(ret == ESP_OK) {
        return IMU_OK;
    }
    return IMU_FAILED;
}


void esp32_imu_init(const imu_params_t* params, qmi8658_bus_t *bus) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = params->MOSI_PIN,
        .miso_io_num = params->MISO_PIN,
        .sclk_io_num = params->CLK_PIN,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
        .max_transfer_sz = 1500
    };
    esp_err_t ret = spi_bus_initialize(SENSOR_IMU_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = params->freq,
        .mode = 0,
        .spics_io_num = params->CS_PIN,
        .address_bits = 0,
        .cs_ena_pretrans = 1,
        .cs_ena_posttrans = 1,
        .input_delay_ns = 50,
        .queue_size = 5,
    };
    ret = spi_bus_add_device(SENSOR_IMU_HOST, &dev_cfg, &spi_device);
    ESP_ERROR_CHECK(ret);
    bus->intf_ptr = (void *) spi_device;    
    bus->read = esp32_imu_reg_read;
    bus->read_fifo = esp32_imu_fifo_read; 
    bus->write = esp32_imu_reg_write;
}

void esp32_imu_deinit(void) {
    if (spi_device != NULL) {
        esp_err_t ret = spi_bus_remove_device(spi_device);
        if (ret == ESP_OK) {
            spi_device = NULL; 
        } else {
            ESP_LOGE(TAG, "Invalid spi handle");
        }
    }
    spi_bus_free(SENSOR_IMU_HOST);
}
