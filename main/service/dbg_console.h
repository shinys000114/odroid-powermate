#ifndef ODROID_POWER_MATE_DBG_CONSOLE_H
#define ODROID_POWER_MATE_DBG_CONSOLE_H

#include "esp_err.h"

/**
 * @brief Initialize the debug console.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t initialize_dbg_console(void);

#endif // ODROID_POWER_MATE_DBG_CONSOLE_H
