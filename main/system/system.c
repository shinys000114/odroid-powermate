//
// Created by shinys on 25. 8. 5.
//

#include <system.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "odroid";
int t = 0;

TaskHandle_t reboot_handle = NULL;

static void reboot_task(void *arg)
{
    while (t > 0)
    {
        ESP_LOGW(TAG, "ESP will reboot in [%d] sec..., If you want stop reboot, use command \"reboot -s\"", t);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        --t;
    }
    esp_restart();
}

void start_reboot_timer(int sec)
{

    if (reboot_handle != NULL)
    {
        ESP_LOGW(TAG, "The reboot timer is already running.");
        return;
    }
    t = sec;
    xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 8, &reboot_handle);
}

void stop_reboot_timer()
{
    if (reboot_handle == NULL)
    {
        return;
    }
    vTaskDelete(reboot_handle);
}