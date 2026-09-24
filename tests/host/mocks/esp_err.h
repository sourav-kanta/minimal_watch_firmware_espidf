#ifndef ESP_ERR_H
#define ESP_ERR_H

#define ESP_OK 0
#define ESP_FAIL -1

#define esp_err_to_name(err) ((err) == ESP_OK ? "ESP_OK" : "ESP_ERROR")

#endif /* ESP_ERR_H */
