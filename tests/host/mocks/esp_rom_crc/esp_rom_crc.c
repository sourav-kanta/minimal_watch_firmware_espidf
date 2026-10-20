#include <esp_rom_crc.h>

uint32_t esp_rom_crc32_le(uint32_t crc,
                          const uint8_t *buf,
                          uint32_t len) {
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            }
            else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}
