#ifndef HOST_ESP_SYSTEM_H
#define HOST_ESP_SYSTEM_H

#include <stdlib.h>

static inline void esp_system_abort(const char *details) {
    (void)details;
    abort();
}

#endif
