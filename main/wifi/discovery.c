/*
 * SPDX-FileCopyrightText: 2026 Hardkernel Co., Ltd.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "esp_log.h"
#include "mdns.h"
#include "sdkconfig.h"
#include "wifi.h"

static const char* TAG = "DISCOVERY";

esp_err_t discovery_start(void)
{
    // Answers <hostname>.local on both the STA network and the setup AP, under the same
    // name the PowerMate already gives DHCP.
    esp_err_t err = mdns_init();
    if (err == ESP_OK)
        err = mdns_hostname_set(CONFIG_LWIP_LOCAL_HOSTNAME);
    if (err == ESP_OK)
        err = mdns_instance_name_set("ODROID PowerMate");
    if (err == ESP_OK)
        err = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "mDNS unavailable: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "mDNS: http://%s.local/", CONFIG_LWIP_LOCAL_HOSTNAME);
    return ESP_OK;
}
