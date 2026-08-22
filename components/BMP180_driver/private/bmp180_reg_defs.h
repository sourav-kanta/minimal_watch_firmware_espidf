#ifndef BMP180_REG_DEFS_H
#define BMP180_REG_DEFS_H

/* ========================================================================== */
/*                       Calibration Registers (EEPROM)                       */
/* ========================================================================== */
/* Note: The sensor outputs 16-bit values MSB first.                          */
/* 0xAA contains the High Byte (MSB), 0xAB contains the Low Byte (LSB).      */

#define BMP180_REG_AC1_H                    0xAA
#define BMP180_REG_AC1_L                    0xAB

#define BMP180_REG_AC2_H                    0xAC
#define BMP180_REG_AC2_L                    0xAD

#define BMP180_REG_AC3_H                    0xAE
#define BMP180_REG_AC3_L                    0xAF

#define BMP180_REG_AC4_H                    0xB0
#define BMP180_REG_AC4_L                    0xB1

#define BMP180_REG_AC5_H                    0xB2
#define BMP180_REG_AC5_L                    0xB3

#define BMP180_REG_AC6_H                    0xB4
#define BMP180_REG_AC6_L                    0xB5

#define BMP180_REG_B1_H                     0xB6
#define BMP180_REG_B1_L                     0xB7

#define BMP180_REG_B2_H                     0xB8
#define BMP180_REG_B2_L                     0xB9

#define BMP180_REG_MB_H                     0xBA
#define BMP180_REG_MB_L                     0xBB

#define BMP180_REG_MC_H                     0xBC
#define BMP180_REG_MC_L                     0xBD

#define BMP180_REG_MD_H                     0xBE
#define BMP180_REG_MD_L                     0xBF

/* ========================================================================== */
/*                     Control & Output Data Registers                        */
/* ========================================================================== */

#define BMP180_REG_CONTROL                  0xF4  /* Measurement control register */
#define BMP180_REG_DATA_MSB                 0xF6  /* Output data MSB              */
#define BMP180_REG_DATA_LSB                 0xF7  /* Output data LSB              */
#define BMP180_REG_DATA_XLSB                0xF8  /* Output data XLSB (Pressure)  */
#define BMP180_REG_CHIP_ID                  0xD0  /* Chip ID verification register*/

/* ========================================================================== */
/*                        Control Register Commands                           */
/* ========================================================================== */

#define BMP180_CMD_READ_TEMP                0x2E  /* Trigger temperature measurement */
#define BMP180_CMD_READ_PRESS               0x34  /* Base pressure command template  */

/* ========================================================================== */
/*                                  Bitmasks                                  */
/* ========================================================================== */

/* Oversampling Setting (OSS) Mask for Control Register (Bits 7 & 6) */
#define BMP180_OSS_MASK                     0xC0  
#define BMP180_OSS_SHIFT                    6

/* Expected values */
#define BMP180_CHIP_ID_VALUE                0x55  /* Fixed verification signature    */

#endif /* BMP180_REG_DEFS_H */

