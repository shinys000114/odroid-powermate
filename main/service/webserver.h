//
// Created by shinys on 25. 8. 18..
//

#ifndef ODROID_REMOTE_HTTP_WEBSERVER_H
#define ODROID_REMOTE_HTTP_WEBSERVER_H
#include "cJSON.h"
#include "esp_http_server.h"

void register_wifi_endpoint(httpd_handle_t server);
void register_ws_endpoint(httpd_handle_t server);
void register_control_endpoint(httpd_handle_t server);
void push_data_to_ws(cJSON* data);
void register_reboot_endpoint(httpd_handle_t server);
esp_err_t change_baud_rate(int baud_rate);

#endif //ODROID_REMOTE_HTTP_WEBSERVER_H
