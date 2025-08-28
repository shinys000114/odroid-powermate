//
// Created by shinys on 25. 7. 10.
//

#ifndef WIFI_H
#define WIFI_H
#include "esp_err.h"
#include "esp_netif_types.h"
#include "esp_wifi_types_generic.h"

const char* auth_mode_str(wifi_auth_mode_t mode);
esp_err_t wifi_connect(void);
esp_err_t wifi_disconnect(void);
void wifi_scan_aps(wifi_ap_record_t * *ap_records, uint16_t * count);
esp_err_t wifi_get_current_ap_info(wifi_ap_record_t * ap_info);
esp_err_t wifi_get_current_ip_info(esp_netif_ip_info_t * ip_info);
esp_err_t wifi_get_dns_info(esp_netif_dns_type_t type, esp_netif_dns_info_t* dns_info);
esp_err_t wifi_use_dhcp(void);
esp_err_t wifi_use_static(const char* ip, const char* gw, const char* netmask, const char* dns1, const char* dns2);
esp_err_t wifi_switch_mode(const char* mode);
void sync_time();

#endif //WIFI_H