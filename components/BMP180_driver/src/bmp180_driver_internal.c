#include <bmp180_internal.h>
#include <driver/i2c_master.h>
#include <gpio_pins.h>
#include <esp_err.h>
#include <esp_log.h>

#define SENSOR_I2C_ADDR 0x77
#define I2C_MASTER_FREQ_HZ 100*1000
#define I2C_TIMEOUT_MS 30

static const char* TAG = "BMP180 HAL";

bool i2c_read_register(void* intf, uint8_t reg, uint8_t* data, size_t size) {
    i2c_master_dev_handle_t i2c_handle = intf;
    if(i2c_handle == NULL || data == NULL) {
        ESP_LOGE(TAG, "Invalid register read");
        return false;
    }
    esp_err_t err = i2c_master_transmit_receive(i2c_handle, &reg, sizeof(uint8_t), data, size, I2C_TIMEOUT_MS);
    return err == ESP_OK;
}

bool i2c_write_register(void* intf, uint8_t reg, const uint8_t* data, size_t size) {
    i2c_master_dev_handle_t i2c_handle = intf;
    if(i2c_handle == NULL || data == NULL || size == 0) {
        ESP_LOGE(TAG, "Invalid register write");
        return false;
    }
    uint8_t buff[size+1];
    buff[0] = reg;
    memcpy(&buff[1], data, size);
    esp_err_t err = i2c_master_transmit(i2c_handle, buff, sizeof(buff)/sizeof(buff[0]), I2C_TIMEOUT_MS);
    return err == ESP_OK;
}

void bmp180_configure_i2c(bmp180_bus_t* intf) {
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = -1,
        .scl_io_num = SENSOR_I2C_SCL_PIN,
        .sda_io_num = SENSOR_I2C_SDA_PIN,
        .flags.enable_internal_pullup = false
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SENSOR_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
    ESP_LOGI(TAG, "I2C initialized and device added successfully.");

    intf->handle = (void *) dev_handle;
    intf->bus_handle = (void *) bus_handle;
    intf->read_reg = i2c_read_register;
    intf->write_reg = i2c_write_register;
}

void bmp180_release_i2c(bmp180_bus_t *intf) {
    if (intf == NULL) {
        ESP_LOGE(TAG, "Invalid interface to release");
        return;
    }
    
    i2c_master_dev_handle_t dev_handle = (i2c_master_dev_handle_t)intf->handle;
    i2c_master_bus_handle_t bus_handle = (i2c_master_bus_handle_t)intf->bus_handle;
    assert(dev_handle);
    assert(bus_handle);

    if (dev_handle != NULL) {
        esp_err_t err = i2c_master_bus_rm_device(dev_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "I2C device removed successfully.");
            intf->handle = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to remove I2C device: %s", esp_err_to_name(err));
        }
    }
    if (bus_handle != NULL) {
        esp_err_t err = i2c_del_master_bus(bus_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "I2C master bus deleted successfully.");
            intf->bus_handle = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to delete I2C master bus: %s", esp_err_to_name(err));
        }
    }
}
