#include <imu_driver.h>
#include <imu_driver_HAL.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <imu_register_defs.h>
#include <esp_log.h>

static qmi8658_bus_t imu_interface;
static const char* TAG = "IMU Driver";
static const uint8_t CTRL7_DISABLE_ALL_SENSORS = 0x00;

static const qmi8658_cmd_t init_cmds[] = {
    { 
        .reg_addr = QMI8658A_REG_CTRL1,
        .data = QMI8658A_CTRL1_ADDR_AI_MASK |
                QMI8658A_CTRL1_INT1_EN_MASK |
                QMI8658A_CTRL1_INT2_EN_MASK, 
        .delay_ms = 10
    },
    { 
        .reg_addr = QMI8658A_REG_CTRL8,
        .data = QMI8658A_CTRL8_CTRL9_HANDSHAKE_TYPE |
                QMI8658A_CTRL8_ACTIVITY_INT_SEL,
        .delay_ms = 0
    },
    { 
        .reg_addr = QMI8658A_REG_FIFO_CTRL,
        .data = QMI8658A_FIFO_MODE_STREAM |
                QMI8658A_FIFO_SIZE_128_SAMPLES,
        .delay_ms = 0
    },
    { 
        .reg_addr = QMI8658A_REG_CTRL2,
        .data = QMI8658A_ACCEL_FS_8G |
                QMI8658A_ACCEL_ODR_LP_21HZ,
        .delay_ms = 0
    },
    { 
        .reg_addr = QMI8658A_REG_CTRL7,
        .data = QMI8658A_CTRL7_DRDY_DIS_MASK |
                QMI8658A_CTRL7_AEN_MASK,
        .delay_ms = 5
    },
};

static const qmi8658_cmd_t high_accuracy_cmds[] = {
    { 
        .reg_addr = QMI8658A_REG_CTRL7,
        .data = CTRL7_DISABLE_ALL_SENSORS,
        .delay_ms = 1 
    },
    { 
        .reg_addr = QMI8658A_REG_CTRL2,
        .data = QMI8658A_ACCEL_FS_8G |
                QMI8658A_ACCEL_ODR_62_5HZ,
        .delay_ms = 0
    },
    { 
        .reg_addr = QMI8658A_REG_CTRL3,
        .data = QMI8658A_GYRO_FS_1024DPS |
                QMI8658A_GYRO_ODR_56_05HZ,
        .delay_ms = 5
    },
    { 
        .reg_addr = QMI8658A_REG_CTRL7,
        .data = QMI8658A_CTRL7_DRDY_DIS_MASK |
                QMI8658A_CTRL7_AEN_MASK |
                QMI8658A_CTRL7_GEN_MASK,
        .delay_ms = 0
    },
};

static const qmi8658_cmd_t lp_accel_only_cmds[] = {
    { 
        .reg_addr = QMI8658A_REG_CTRL7,
        .data = CTRL7_DISABLE_ALL_SENSORS,
        .delay_ms = 1 
    },
    { 
        .reg_addr = QMI8658A_REG_CTRL2,
        .data = QMI8658A_ACCEL_FS_8G |
                QMI8658A_ACCEL_ODR_LP_21HZ,
        .delay_ms = 5
    },
    { 
        .reg_addr = QMI8658A_REG_CTRL7,
        .data = (QMI8658A_CTRL7_DRDY_DIS_MASK |
                 QMI8658A_CTRL7_AEN_MASK) &
                 (~QMI8658A_CTRL7_GEN_MASK),
        .delay_ms = 0
    },

};

static imu_err_t execute_sequential_cmds(const qmi8658_cmd_t *cmds, int cmd_len) {
    assert(cmd_len!=0);
    assert(cmds);
    imu_err_t success = IMU_OK;
    for(int i=0; i<cmd_len;i++) {
        success = imu_interface.write(imu_interface.intf_ptr, cmds[i].reg_addr,
                                      &cmds[i].data, sizeof(uint8_t));
        if(success != IMU_OK) return success;
        if(cmds[i].delay_ms != 0) {
            TickType_t ticks = pdMS_TO_TICKS(cmds[i].delay_ms);
            vTaskDelay(ticks == 0 ? 1 : ticks);
        }
    }
    return success;
} 

static imu_err_t ctrl9_handshake(uint8_t cmd) {
    imu_err_t success = IMU_OK;
    const int timeout_ms = 20;
    const int delay_ms = 1;
    int elapsed_ms = 0;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CTRL9, &cmd, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    uint8_t ack_cmd = QMI8658A_CTRL9_CMD_ACK;
    bool ack_sent = false;
    while(elapsed_ms < timeout_ms) {
        uint8_t status = 0;
        success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_STATUSINT, 
                                     &status, sizeof(uint8_t));
        if(success != IMU_OK) return success;
        if(ack_sent && 
           ((status & QMI8658A_STATUSINT_CMD_DONE_MASK) == 0x00)) {
            return IMU_OK;
        }
        if(!ack_sent && 
           ((status & QMI8658A_STATUSINT_CMD_DONE_MASK) == QMI8658A_STATUSINT_CMD_DONE_MASK)) {
            success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CTRL9, 
                                          &ack_cmd, sizeof(uint8_t));
            if(success != IMU_OK) return success;
            ack_sent = true; 
        }
        TickType_t ticks = pdMS_TO_TICKS(delay_ms);
        vTaskDelay(ticks == 0 ? 1 : ticks);
        elapsed_ms += delay_ms;
    }
    ESP_LOGE(TAG, "Timeout in CTRL9 handshake cmd : 0x%02X. Skipping", cmd);
    return IMU_FAILED;
}

imu_err_t imu_init(const imu_params_t* params) {
    esp32_imu_init(params, &imu_interface);
    int cmd_len = sizeof(init_cmds) / sizeof(qmi8658_cmd_t);
    imu_err_t success = IMU_OK;
    uint8_t dev = 0;
    success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_WHO_AM_I, &dev, sizeof(uint8_t));
    if(success!= IMU_OK || dev != QMI8658A_WHO_AM_I_VAL) {
        ESP_LOGE(TAG, "Failed to verify IMU identity");
        return IMU_FAILED;
    }
    success = execute_sequential_cmds(init_cmds, cmd_len);
    return success;    
}

imu_err_t imu_reset(void) {
    qmi8658_cmd_t reset_cmd = {
        .reg_addr = QMI8658A_REG_RESET,
        .data = QMI8658A_RESET_CMD,
        .delay_ms = 15
    };
    imu_err_t success = IMU_OK;
    success = imu_interface.write(imu_interface.intf_ptr, reset_cmd.reg_addr, 
                                  &reset_cmd.data, sizeof(uint8_t));
    if(success != IMU_OK) {
        ESP_LOGE(TAG, "Failed to write reset command");
        return success;
    }
    TickType_t ticks = pdMS_TO_TICKS(reset_cmd.delay_ms);
    vTaskDelay(ticks == 0 ? 1 : ticks);
    uint8_t dev = 0;
    success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_WHO_AM_I, &dev, sizeof(uint8_t));
    if(success!= IMU_OK || dev != QMI8658A_WHO_AM_I_VAL) {
        ESP_LOGE(TAG, "Failed to verify IMU identity");
        return IMU_FAILED;
    }
    return success;
}

imu_err_t imu_enable_accelerometer(void) {
    uint8_t data = 0;
    imu_err_t success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_CTRL7, 
                                           &data, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    data = data | QMI8658A_CTRL7_AEN_MASK;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CTRL7, &data, sizeof(uint8_t));
    return success;
}

imu_err_t imu_disable_accelerometer(void) {
    uint8_t data = 0;
    imu_err_t success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_CTRL7, 
                                           &data, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    data = data & (~QMI8658A_CTRL7_AEN_MASK);
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CTRL7, &data, sizeof(uint8_t));
    return success;
}

imu_err_t imu_enable_gyro(void) {
    uint8_t data = 0;
    imu_err_t success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_CTRL7, 
                                           &data, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    data = data | QMI8658A_CTRL7_GEN_MASK;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CTRL7, &data, sizeof(uint8_t));
    return success;
}

imu_err_t imu_disable_gyro(void) {
    uint8_t data = 0;
    imu_err_t success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_CTRL7, 
                                           &data, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    data = data & (~QMI8658A_CTRL7_GEN_MASK);
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CTRL7, &data, sizeof(uint8_t));
    return success;
} 

imu_err_t imu_configure_high_accuracy_mode(void) {
    int cmd_len = sizeof(high_accuracy_cmds) / sizeof(qmi8658_cmd_t);
    return execute_sequential_cmds(high_accuracy_cmds, cmd_len);
}

imu_err_t imu_enter_low_power_accel_only_mode(void) {
    int cmd_len = sizeof(lp_accel_only_cmds) / sizeof(qmi8658_cmd_t);
    return execute_sequential_cmds(lp_accel_only_cmds, cmd_len);
}

imu_err_t imu_enter_low_power_mode(void) {
    imu_err_t success = IMU_OK;
    success = imu_disable_accelerometer();
    if(success != IMU_OK) return success;
    success = imu_disable_gyro();
    return success;
}

imu_err_t imu_enable_pedometer(void) {
    imu_err_t success = IMU_OK;
    uint8_t data = 0;
    success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_CTRL7, &data, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    bool accel_enabled = (data & QMI8658A_CTRL7_AEN_MASK) == QMI8658A_CTRL7_AEN_MASK;
    bool gyro_enabled = (data & QMI8658A_CTRL7_GEN_MASK) == QMI8658A_CTRL7_GEN_MASK;
    success = imu_enter_low_power_mode();
    if(success != IMU_OK) return success;
    uint16_t sample_cnt = 10;
    uint16_t peak2peak = 0x0140;    //312mg
    uint16_t peak = 0x00A0;         //156mg
    uint16_t time_up = 31;          //1.5s @21hz
    uint8_t time_low = 6;           //0.25s @21hz
    uint8_t time_cnt_entry = 5;
    uint8_t fix_precision = 0;
    uint8_t sig_count = 1;
    uint8_t sample_cnt_l = sample_cnt & 0xFF;
    uint8_t sample_cnt_h = sample_cnt >> 8;
    uint8_t peak2peak_l = peak2peak & 0xFF;
    uint8_t peak2peak_h = peak2peak >> 8;
    uint8_t peak_l = peak & 0xFF;
    uint8_t peak_h = peak >> 8;
    uint8_t time_up_l = time_up & 0xFF;
    uint8_t time_up_h = time_up >> 8;
    uint8_t cmd1 = 0x01;
    uint8_t cmd2 = 0x02;

    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL1_L, 
                                  &sample_cnt_l, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL1_H, 
                                  &sample_cnt_h, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL2_L, 
                                  &peak2peak_l, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL2_H, 
                                  &peak2peak_h, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL3_L, 
                                  &peak_l, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL3_H, 
                                  &peak_h, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL4_H, 
                                  &cmd1, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = ctrl9_handshake(QMI8658A_CTRL9_CMD_CONFIGURE_PEDOMETER);
    if(success != IMU_OK) return success;

    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL1_L, 
                                  &time_up_l, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL1_H, 
                                  &time_up_h, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL2_L, 
                                  &time_low, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL2_H, 
                                  &time_cnt_entry, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL3_L, 
                                  &fix_precision, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL3_H, 
                                  &sig_count, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CAL4_H, 
                                  &cmd2, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = ctrl9_handshake(QMI8658A_CTRL9_CMD_CONFIGURE_PEDOMETER);
    if(success != IMU_OK) return success;

    data = 0;
    success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_CTRL8, &data, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    data = data | QMI8658A_CTRL8_PEDO_EN_MASK;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CTRL8, &data, sizeof(uint8_t));
    if(success != IMU_OK) return success;

    if(accel_enabled) success = imu_enable_accelerometer();
    else ESP_LOGW(TAG, "Pedometer enabled but accelerometer is off, step counting wont work");
    if(success != IMU_OK) return success;
    if(gyro_enabled) success = imu_enable_gyro();
    return success;
}

imu_err_t imu_disable_pedometer(void) {
    uint8_t data = 0;
    imu_err_t success = IMU_OK;
    success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_CTRL8, &data, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    data = data & (~QMI8658A_CTRL8_PEDO_EN_MASK);
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_CTRL8, &data, sizeof(uint8_t));
    return success;
}

imu_err_t imu_reset_pedometer(void) {
    return ctrl9_handshake(QMI8658A_CTRL9_CMD_RESET_PEDOMETER);
}

imu_err_t imu_read_pedometer_steps(uint32_t* out_steps) {
    if(!out_steps) return IMU_INVALID_CONFIG;
    imu_err_t success = IMU_OK;
    uint32_t steps = 0;
    uint8_t data[3] = {0};
    success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_STEP_CNT_LOW, data, sizeof(uint8_t)*3);
    if(success != IMU_OK) return success;
    steps = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
    *out_steps = steps;
    return success;
}

imu_err_t imu_read_fifo_buffer(uint8_t* buff, size_t* len) {
    imu_err_t success = IMU_OK;
    if(!buff || !len) return IMU_INVALID_CONFIG;
    size_t max_len = *len;
    uint8_t count_bytes[2] = {0};
    success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_FIFO_SMPL_CNT, 
                                 count_bytes, sizeof(count_bytes)/sizeof(count_bytes[0]));
    if(success != IMU_OK) return success;
    *len = (((count_bytes[1]&0b11)<<8) | count_bytes[0])*2; // QMI stores count in 2 byte word
    if(*len == 0) return success;
    size_t read_len = max_len > *len ? *len : max_len;    
    uint8_t fifo_ctrl = 0;
    success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_FIFO_CTRL, &fifo_ctrl, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    fifo_ctrl |= QMI8658A_FIFO_CTRL_RD_MODE_MASK;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_FIFO_CTRL, &fifo_ctrl, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = imu_interface.read_fifo(imu_interface.intf_ptr, QMI8658A_REG_FIFO_DATA, buff, read_len);
    if(success != IMU_OK) return success;
    *len = read_len;
    fifo_ctrl &= ~QMI8658A_FIFO_CTRL_RD_MODE_MASK; 
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_FIFO_CTRL, &fifo_ctrl, sizeof(uint8_t));
    return success;
}

imu_err_t imu_reset_fifo_buffer(void) {
    uint8_t fifo_ctrl;
    imu_err_t success = IMU_OK;
    success = imu_interface.read(imu_interface.intf_ptr, QMI8658A_REG_FIFO_CTRL, &fifo_ctrl, sizeof(uint8_t));
    if(success != IMU_OK) return success;
    success = ctrl9_handshake(QMI8658A_CTRL9_CMD_RST_FIFO);
    if(success != IMU_OK) return success;
    success = imu_interface.write(imu_interface.intf_ptr, QMI8658A_REG_FIFO_CTRL, &fifo_ctrl, sizeof(uint8_t));
    return success;
}

imu_err_t imu_setup_wake_on_motion(void) {
    ESP_LOGE(TAG, "Unimplemented : TODO");
    return IMU_FAILED;
}

imu_err_t imu_setup_detect_no_motion(void) {
    ESP_LOGE(TAG, "Unimplemented : TODO");
    return IMU_FAILED;
}


imu_err_t imu_disable_detect_no_motion(void) {
    ESP_LOGE(TAG, "Unimplemented : TODO");
    return IMU_FAILED;
}

imu_err_t imu_disable_wake_on_motion(void) {
    ESP_LOGE(TAG, "Unimplemented : TODO");
    return IMU_FAILED;
}

