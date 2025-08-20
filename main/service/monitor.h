//
// Created by shinys on 25. 8. 18..
//

#ifndef ODROID_REMOTE_HTTP_MONITOR_H
#define ODROID_REMOTE_HTTP_MONITOR_H

#include <stdint.h>

#include "esp_http_server.h"

// 버퍼에 저장할 데이터의 개수
#define SENSOR_BUFFER_SIZE 100

// 단일 센서 데이터를 저장하기 위한 구조체
typedef struct {
    float voltage;
    float current;
    float power;
    uint32_t timestamp; // 데이터를 읽은 시간 (부팅 후 ms)
} sensor_data_t;

void init_status_monitor();

#endif //ODROID_REMOTE_HTTP_MONITOR_H