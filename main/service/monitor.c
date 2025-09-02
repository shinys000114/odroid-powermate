//
// Created by shinys on 25. 8. 18..
//

#include "monitor.h"
#include <time.h>
#include "datalog.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "ina3221.h"
#include "pb.h"
#include "pb_encode.h"
#include "status.pb.h"
#include "webserver.h"
#include "wifi.h"

#define PM_SDA CONFIG_I2C_GPIO_SDA
#define PM_SCL CONFIG_I2C_GPIO_SCL

#define PB_BUFFER_SIZE 256

static const char* TAG = "monitor";

ina3221_t ina3221 = {
    .shunt = {10, 10, 10},
    .mask.mask_register = INA3221_DEFAULT_MASK,
    .i2c_dev = {0},
    .config =
        {
            .mode = true, // mode selection
            .esht = true, // shunt enable
            .ebus = true, // bus enable
            .ch1 = true, // channel 1 enable
            .ch2 = true, // channel 2 enable
            .ch3 = true, // channel 3 enable
            .avg = INA3221_AVG_64, // 64 samples average
            .vbus = INA3221_CT_2116, // 2ms by channel (bus)
            .vsht = INA3221_CT_2116, // 2ms by channel (shunt)
        },
};

static bool encode_string(pb_ostream_t* stream, const pb_field_t* field, void* const* arg)
{
    const char* str = (const char*)(*arg);
    if (!str)
    {
        return true; // Nothing to encode
    }
    if (!pb_encode_tag_for_field(stream, field))
    {
        return false;
    }
    return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}

static void send_pb_message(const pb_msgdesc_t* fields, const void* src_struct)
{
    uint8_t buffer[PB_BUFFER_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (!pb_encode(&stream, fields, src_struct))
    {
        ESP_LOGE(TAG, "Failed to encode protobuf message: %s", PB_GET_ERROR(&stream));
        return;
    }

    push_data_to_ws(buffer, stream.bytes_written);
}

static void sensor_timer_callback(void* arg)
{
    int64_t uptime_us = esp_timer_get_time();
    uint32_t uptime_sec = (uint32_t)(uptime_us / 1000000);
    uint32_t timestamp = (uint32_t)time(NULL);

    channel_data_t channel_data_log[NUM_CHANNELS];

    StatusMessage message = StatusMessage_init_zero;
    message.which_payload = StatusMessage_sensor_data_tag;
    SensorData* sensor_data = &message.payload.sensor_data;

    sensor_data->has_usb = true;
    sensor_data->has_main = true;
    sensor_data->has_vin = true;

    SensorChannelData* channels[] = {&sensor_data->usb, &sensor_data->main, &sensor_data->vin};

    for (uint8_t i = 0; i < INA3221_BUS_NUMBER; i++)
    {
        float voltage, current, power;
        ina3221_get_bus_voltage(&ina3221, i, &voltage);
        ina3221_get_shunt_value(&ina3221, i, NULL, &current);

        current /= 1000.0f; // mA to A
        power = voltage * current;

        // For datalog
        channel_data_log[i] = (channel_data_t){.voltage = voltage, .current = current, .power = power};

        // For protobuf
        channels[i]->voltage = voltage;
        channels[i]->current = current;
        channels[i]->power = power;
    }

    // datalog_add(timestamp, channel_data_log);

    sensor_data->timestamp = timestamp;
    sensor_data->uptime_sec = uptime_sec;

    send_pb_message(StatusMessage_fields, &message);
}

static void status_wifi_callback(void* arg)
{
    wifi_ap_record_t ap_info;
    StatusMessage message = StatusMessage_init_zero;
    message.which_payload = StatusMessage_wifi_status_tag;
    WifiStatus* wifi_status = &message.payload.wifi_status;

    if (wifi_get_current_ap_info(&ap_info) == ESP_OK)
    {
        wifi_status->connected = true;
        wifi_status->ssid.funcs.encode = &encode_string;
        wifi_status->ssid.arg = (void*)ap_info.ssid;
        wifi_status->rssi = ap_info.rssi;
    }
    else
    {
        wifi_status->connected = false;
        wifi_status->ssid.arg = ""; // Empty string
        wifi_status->rssi = 0;
    }

    send_pb_message(StatusMessage_fields, &message);
}

static esp_timer_handle_t sensor_timer;
static esp_timer_handle_t wifi_status_timer;

void init_status_monitor()
{
    ESP_ERROR_CHECK(ina3221_init_desc(&ina3221, 0x40, 0, PM_SDA, PM_SCL));
    datalog_init();

    const esp_timer_create_args_t sensor_timer_args = {.callback = &sensor_timer_callback,
                                                       .name = "sensor_reading_timer"};
    const esp_timer_create_args_t wifi_timer_args = {.callback = &status_wifi_callback, .name = "wifi_status_timer"};

    ESP_ERROR_CHECK(esp_timer_create(&sensor_timer_args, &sensor_timer));
    ESP_ERROR_CHECK(esp_timer_create(&wifi_timer_args, &wifi_status_timer));

    ESP_ERROR_CHECK(esp_timer_start_periodic(sensor_timer, 1000000));
    ESP_ERROR_CHECK(esp_timer_start_periodic(wifi_status_timer, 1000000 * 5));
}
