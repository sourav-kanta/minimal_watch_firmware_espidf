#ifndef IMU_REGISTER_DEFS_H
#define IMU_REGISTER_DEFS_H

#define QMI8658A_WHO_AM_I_VAL               0x05    /**< Expected WHO_AM_I value */
#define QMI8658A_REVISION_ID                0x7C    /**< Expected Revision id value */
#define QMI8658A_RESET_CMD                  0xB0    /**< Soft reset command value */

/**
 * I2C Slave Addresses (SA0 Pin dependent)
 */
#define QMI8658A_I2C_ADDR_SA0_HIGH          0x6A    /**< SA0 connected to VDDIO / High */
#define QMI8658A_I2C_ADDR_SA0_LOW           0x6B    /**< SA0 connected to GND / Low */

/* ========================================================================= */
/* Register Addresses Map                                                    */
/* ========================================================================= */

/* General Purpose / Information Registers */
#define QMI8658A_REG_WHO_AM_I               0x00    /**< Device Identifier */
#define QMI8658A_REG_REVISION_ID            0x01    /**< Device Revision ID */

/* Setup and Control Registers */
#define QMI8658A_REG_CTRL1                  0x02    /**< SPI Interface and Sensor Enable */
#define QMI8658A_REG_CTRL2                  0x03    /**< Accelerometer Settings */
#define QMI8658A_REG_CTRL3                  0x04    /**< Gyroscope Settings */
#define QMI8658A_REG_CTRL5                  0x06    /**< Low Pass Filter Settings */
#define QMI8658A_REG_CTRL7                  0x08    /**< Enable Sensors */
#define QMI8658A_REG_CTRL8                  0x09    /**< Motion Detection Control */
#define QMI8658A_REG_CTRL9                  0x0A    /**< Host Commands */

/* Host Controlled Calibration Registers */
#define QMI8658A_REG_CAL1_L                 0x0B
#define QMI8658A_REG_CAL1_H                 0x0C
#define QMI8658A_REG_CAL2_L                 0x0D
#define QMI8658A_REG_CAL2_H                 0x0E
#define QMI8658A_REG_CAL3_L                 0x0F
#define QMI8658A_REG_CAL3_H                 0x10
#define QMI8658A_REG_CAL4_L                 0x11
#define QMI8658A_REG_CAL4_H                 0x12

/* FIFO Registers */
#define QMI8658A_REG_FIFO_WTM_TH            0x13    /**< FIFO Watermark Level */
#define QMI8658A_REG_FIFO_CTRL              0x14    /**< FIFO Setup */
#define QMI8658A_REG_FIFO_SMPL_CNT          0x15    /**< FIFO Sample Count LSBs */
#define QMI8658A_REG_FIFO_STATUS            0x16    /**< FIFO Status */
#define QMI8658A_REG_FIFO_DATA              0x17    /**< FIFO Data */

/* Status Registers */
#define QMI8658A_REG_STATUSINT              0x2D    /**< Data Availability & Lock Status */
#define QMI8658A_REG_STATUS0                0x2E    /**< Sensor Data Availability Flags */
#define QMI8658A_REG_STATUS1                0x2F    /**< Motion/Activity Status Flags */

/* Timestamp Registers */
#define QMI8658A_REG_TIMESTAMP_L            0x30    /**< Time Stamp LSB */
#define QMI8658A_REG_TIMESTAMP_M            0x31    /**< Time Stamp MID */
#define QMI8658A_REG_TIMESTAMP_H            0x32    /**< Time Stamp MSB */

/* Sensor Data Output Registers */
#define QMI8658A_REG_TEMP_L                 0x33    /**< Temperature Output LSB */
#define QMI8658A_REG_TEMP_H                 0x34    /**< Temperature Output MSB */
#define QMI8658A_REG_AX_L                   0x35    /**< Accelerometer X-axis LSB */
#define QMI8658A_REG_AX_H                   0x36    /**< Accelerometer X-axis MSB */
#define QMI8658A_REG_AY_L                   0x37    /**< Accelerometer Y-axis LSB */
#define QMI8658A_REG_AY_H                   0x38    /**< Accelerometer Y-axis MSB */
#define QMI8658A_REG_AZ_L                   0x39    /**< Accelerometer Z-axis LSB */
#define QMI8658A_REG_AZ_H                   0x3A    /**< Accelerometer Z-axis MSB */
#define QMI8658A_REG_GX_L                   0x3B    /**< Gyroscope X-axis LSB */
#define QMI8658A_REG_GX_H                   0x3C    /**< Gyroscope X-axis MSB */
#define QMI8658A_REG_GY_L                   0x3D    /**< Gyroscope Y-axis LSB */
#define QMI8658A_REG_GY_H                   0x3E    /**< Gyroscope Y-axis MSB */
#define QMI8658A_REG_GZ_L                   0x3F    /**< Gyroscope Z-axis LSB */
#define QMI8658A_REG_GZ_H                   0x40    /**< Gyroscope Z-axis MSB */

/* Calibration-On-Demand & General Purpose Registers */
#define QMI8658A_REG_COD_STATUS             0x46    /**< Calibration-On-Demand Status */
#define QMI8658A_REG_DQW_L                  0x49
#define QMI8658A_REG_DQW_H                  0x4A
#define QMI8658A_REG_DQX_L                  0x4B
#define QMI8658A_REG_DQX_H                  0x4C
#define QMI8658A_REG_DQY_L                  0x4D
#define QMI8658A_REG_DQY_H                  0x4E
#define QMI8658A_REG_DQZ_L                  0x4F
#define QMI8658A_REG_DQZ_H                  0x50
#define QMI8658A_REG_DVX_L                  0x51
#define QMI8658A_REG_DVX_H                  0x52
#define QMI8658A_REG_DVY_L                  0x53
#define QMI8658A_REG_DVY_H                  0x54
#define QMI8658A_REG_DVZ_L                  0x55
#define QMI8658A_REG_DVZ_H                  0x56

/* Activity Detection Output Registers */
#define QMI8658A_REG_TAP_STATUS             0x59    /**< Axis, direction, and count of Tap */
#define QMI8658A_REG_STEP_CNT_LOW           0x5A    /**< Pedometer Step Count LSB */
#define QMI8658A_REG_STEP_CNT_MID           0x5B    /**< Pedometer Step Count MID */
#define QMI8658A_REG_STEP_CNT_HIGH          0x5C    /**< Pedometer Step Count MSB */

/* Reset Register */
#define QMI8658A_REG_RESET                  0x60    /**< Soft Reset Register */

/* ========================================================================= */
/* Register Bit Definitions & Field Masks                                    */
/* ========================================================================= */

/* --- CTRL1 (0x02) --- */
#define QMI8658A_CTRL1_SIM_MASK             (1 << 7) /**< 0: 4-wire SPI, 1: 3-wire SPI */
#define QMI8658A_CTRL1_ADDR_AI_MASK         (1 << 6) /**< Address auto-increment */
#define QMI8658A_CTRL1_BE_MASK              (1 << 5) /**< 0: Little-Endian, 1: Big-Endian */
#define QMI8658A_CTRL1_INT2_EN_MASK         (1 << 4) /**< Enable INT2 pin output */
#define QMI8658A_CTRL1_INT1_EN_MASK         (1 << 3) /**< Enable INT1 pin output */
#define QMI8658A_CTRL1_FIFO_INT_SEL_MASK    (1 << 2) /**< 0: INT2, 1: INT1 */
#define QMI8658A_CTRL1_SENSOR_DISABLE_MASK  (1 << 0) /**< 0: Enable High-Speed Osc, 1: Disable */

/* --- CTRL2 (0x03): Accelerometer Config --- */
#define QMI8658A_CTRL2_AST_MASK             (1 << 7) /**< Accel Self-Test Enable */
#define QMI8658A_CTRL2_AFS_MASK             (0x70)   /**< Accel Full Scale [6:4] */
#define QMI8658A_CTRL2_AFS_POS              (4)
#define QMI8658A_CTRL2_AODR_MASK            (0x0F)   /**< Accel ODR [3:0] */
#define QMI8658A_CTRL2_AODR_POS             (0)

typedef enum {
    QMI8658A_ACCEL_FS_2G  = (0x00 << 4),
    QMI8658A_ACCEL_FS_4G  = (0x01 << 4),
    QMI8658A_ACCEL_FS_8G  = (0x02 << 4),
    QMI8658A_ACCEL_FS_16G = (0x03 << 4)
} qmi8658a_accel_fs_t;

typedef enum {
    QMI8658A_ACCEL_ODR_7174_4HZ = 0x00, /* 6DOF Mode */
    QMI8658A_ACCEL_ODR_3587_2HZ = 0x01,
    QMI8658A_ACCEL_ODR_1793_6HZ = 0x02,
    QMI8658A_ACCEL_ODR_1000HZ   = 0x03, /* Normal Mode */
    QMI8658A_ACCEL_ODR_500HZ    = 0x04,
    QMI8658A_ACCEL_ODR_250HZ    = 0x05,
    QMI8658A_ACCEL_ODR_125HZ    = 0x06,
    QMI8658A_ACCEL_ODR_62_5HZ   = 0x07,
    QMI8658A_ACCEL_ODR_31_25HZ  = 0x08,
    QMI8658A_ACCEL_ODR_LP_128HZ = 0x0C, /* Low-Power Mode */
    QMI8658A_ACCEL_ODR_LP_21HZ  = 0x0D,
    QMI8658A_ACCEL_ODR_LP_11HZ  = 0x0E,
    QMI8658A_ACCEL_ODR_LP_3HZ   = 0x0F
} qmi8658a_accel_odr_t;

/* --- CTRL3 (0x04): Gyroscope Config --- */
#define QMI8658A_CTRL3_GST_MASK             (1 << 7) /**< Gyro Self-Test Enable */
#define QMI8658A_CTRL3_GFS_MASK             (0x70)   /**< Gyro Full Scale [6:4] */
#define QMI8658A_CTRL3_GFS_POS              (4)
#define QMI8658A_CTRL3_GODR_MASK            (0x0F)   /**< Gyro ODR [3:0] */
#define QMI8658A_CTRL3_GODR_POS             (0)

typedef enum {
    QMI8658A_GYRO_FS_16DPS   = (0x00 << 4),
    QMI8658A_GYRO_FS_32DPS   = (0x01 << 4),
    QMI8658A_GYRO_FS_64DPS   = (0x02 << 4),
    QMI8658A_GYRO_FS_128DPS  = (0x03 << 4),
    QMI8658A_GYRO_FS_256DPS  = (0x04 << 4),
    QMI8658A_GYRO_FS_512DPS  = (0x05 << 4),
    QMI8658A_GYRO_FS_1024DPS = (0x06 << 4),
    QMI8658A_GYRO_FS_2048DPS = (0x07 << 4)
} qmi8658a_gyro_fs_t;

typedef enum {
    QMI8658A_GYRO_ODR_7174_4HZ = 0x00,
    QMI8658A_GYRO_ODR_3587_2HZ = 0x01,
    QMI8658A_GYRO_ODR_1793_6HZ = 0x02,
    QMI8658A_GYRO_ODR_896_8HZ  = 0x03,
    QMI8658A_GYRO_ODR_448_4HZ  = 0x04,
    QMI8658A_GYRO_ODR_224_2HZ  = 0x05,
    QMI8658A_GYRO_ODR_112_1HZ  = 0x06,
    QMI8658A_GYRO_ODR_56_05HZ  = 0x07,
    QMI8658A_GYRO_ODR_28_025HZ = 0x08
} qmi8658a_gyro_odr_t;

/* --- CTRL5 (0x06): Low-Pass Filter Settings --- */
#define QMI8658A_CTRL5_GLPF_MODE_MASK       (0x60)   /**< Gyro LPF Mode [6:5] */
#define QMI8658A_CTRL5_GLPF_EN_MASK         (1 << 4) /**< Enable Gyro LPF */
#define QMI8658A_CTRL5_ALPF_MODE_MASK       (0x06)   /**< Accel LPF Mode [2:1] */
#define QMI8658A_CTRL5_ALPF_EN_MASK         (1 << 0) /**< Enable Accel LPF */

typedef enum {
    QMI8658A_LPF_MODE_2_66_PERCENT  = 0x00, /**< 2.66% of ODR */
    QMI8658A_LPF_MODE_3_63_PERCENT  = 0x01, /**< 3.63% of ODR */
    QMI8658A_LPF_MODE_5_39_PERCENT  = 0x02, /**< 5.39% of ODR */
    QMI8658A_LPF_MODE_13_37_PERCENT = 0x03  /**< 13.37% of ODR */
} qmi8658a_lpf_mode_t;

/* --- CTRL7 (0x08): Enable Sensors --- */
#define QMI8658A_CTRL7_SYNC_SAMPLE_MASK     (1 << 7) /**< SyncSample Mode */
#define QMI8658A_CTRL7_DRDY_DIS_MASK        (1 << 5) /**< Disable DRDY output on INT2 */
#define QMI8658A_CTRL7_GSN_MASK             (1 << 4) /**< Gyro Snooze Mode */
#define QMI8658A_CTRL7_GEN_MASK             (1 << 1) /**< Enable Gyroscope */
#define QMI8658A_CTRL7_AEN_MASK             (1 << 0) /**< Enable Accelerometer */

/* --- CTRL8 (0x09): Motion Detection Control --- */
#define QMI8658A_CTRL8_CTRL9_HANDSHAKE_TYPE (1 << 7) /**< 0: INT1, 1: STATUSINT bit 7 */
#define QMI8658A_CTRL8_ACTIVITY_INT_SEL     (1 << 6) /**< 0: INT2, 1: INT1 */
#define QMI8658A_CTRL8_PEDO_EN_MASK         (1 << 4) /**< Enable Pedometer */
#define QMI8658A_CTRL8_SIG_MOTION_EN_MASK   (1 << 3) /**< Enable Significant Motion */
#define QMI8658A_CTRL8_NO_MOTION_EN_MASK    (1 << 2) /**< Enable No Motion */
#define QMI8658A_CTRL8_ANY_MOTION_EN_MASK   (1 << 1) /**< Enable Any Motion */
#define QMI8658A_CTRL8_TAP_EN_MASK          (1 << 0) /**< Enable Tap Detection */

/* --- FIFO_CTRL (0x14) --- */
#define QMI8658A_FIFO_CTRL_RD_MODE_MASK     (1 << 7) /**< FIFO Read Mode */
#define QMI8658A_FIFO_CTRL_SIZE_MASK        (0x0C)   /**< FIFO Sample Size [3:2] */
#define QMI8658A_FIFO_CTRL_MODE_MASK        (0x03)   /**< FIFO Mode [1:0] */

typedef enum {
    QMI8658A_FIFO_SIZE_16_SAMPLES  = (0x00 << 2),
    QMI8658A_FIFO_SIZE_32_SAMPLES  = (0x01 << 2),
    QMI8658A_FIFO_SIZE_64_SAMPLES  = (0x02 << 2),
    QMI8658A_FIFO_SIZE_128_SAMPLES = (0x03 << 2)
} qmi8658a_fifo_size_t;

typedef enum {
    QMI8658A_FIFO_MODE_BYPASS = 0x00,
    QMI8658A_FIFO_MODE_FIFO   = 0x01,
    QMI8658A_FIFO_MODE_STREAM = 0x02
} qmi8658a_fifo_mode_t;

/* --- STATUSINT (0x2D) --- */
#define QMI8658A_STATUSINT_CMD_DONE_MASK    (1 << 7) /**< CTRL9 Command Complete */
#define QMI8658A_STATUSINT_LOCKED_MASK      (1 << 1) /**< Sensor Data Locked */
#define QMI8658A_STATUSINT_AVAIL_MASK       (1 << 0) /**< Sensor Data Available */

/* --- STATUS0 (0x2E) --- */
#define QMI8658A_STATUS0_GDA_MASK           (1 << 1) /**< Gyro Data Available */
#define QMI8658A_STATUS0_ADA_MASK           (1 << 0) /**< Accel Data Available */

/* --- STATUS1 (0x2F) --- */
#define QMI8658A_STATUS1_SIGNIFICANT_MOTION (1 << 7)
#define QMI8658A_STATUS1_NO_MOTION          (1 << 6)
#define QMI8658A_STATUS1_ANY_MOTION         (1 << 5)
#define QMI8658A_STATUS1_PEDOMETER          (1 << 4)
#define QMI8658A_STATUS1_WOM                (1 << 2)
#define QMI8658A_STATUS1_TAP                (1 << 1)

/* ========================================================================= */
/* CTRL9 Command Values                                                      */
/* ========================================================================= */
typedef enum {
    QMI8658A_CTRL9_CMD_ACK                   = 0x00, /**< Host Acknowledge */
    QMI8658A_CTRL9_CMD_RST_FIFO              = 0x04, /**< Reset FIFO */
    QMI8658A_CTRL9_CMD_REQ_FIFO              = 0x05, /**< Request/Read FIFO Data */
    QMI8658A_CTRL9_CMD_WRITE_WOM_SETTING     = 0x08, /**< Configure WoM Settings */
    QMI8658A_CTRL9_CMD_ACCEL_HOST_DELTA_OFF  = 0x09, /**< Set Accel Host Delta Offset */
    QMI8658A_CTRL9_CMD_GYRO_HOST_DELTA_OFF   = 0x0A, /**< Set Gyro Host Delta Offset */
    QMI8658A_CTRL9_CMD_CONFIGURE_TAP         = 0x0C, /**< Configure Tap Parameters */
    QMI8658A_CTRL9_CMD_CONFIGURE_PEDOMETER   = 0x0D, /**< Configure Pedometer Parameters */
    QMI8658A_CTRL9_CMD_CONFIGURE_MOTION      = 0x0E, /**< Configure Motion Detection */
    QMI8658A_CTRL9_CMD_RESET_PEDOMETER       = 0x0F, /**< Reset Step Count */
    QMI8658A_CTRL9_CMD_COPY_USID             = 0x10, /**< Copy USID & FW Version */
    QMI8658A_CTRL9_CMD_SET_RPU               = 0x11, /**< Config IO Pull-Ups */
    QMI8658A_CTRL9_CMD_AHB_CLOCK_GATING      = 0x12, /**< AHB Clock Gating Switch */
    QMI8658A_CTRL9_CMD_ON_DEMAND_CALIBRATION = 0xA2, /**< Execute Calibration-On-Demand */
    QMI8658A_CTRL9_CMD_APPLY_GYRO_GAINS      = 0xAA  /**< Restore Gyro Gains */
} qmi8658a_ctrl9_cmd_t;

#endif /* IMU_REGISTER_DEFS_H */
