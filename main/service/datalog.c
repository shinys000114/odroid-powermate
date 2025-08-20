#include "datalog.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_littlefs.h"
#include "esp_log.h"

static const char* TAG = "DATALOG";
static const char* LOG_FILE_PATH = "/littlefs/datalog.csv";
static const char* TEMP_LOG_FILE_PATH = "/littlefs/datalog.tmp";
#define MAX_LOG_SIZE (1024 * 1024)

void datalog_init(void)
{
    ESP_LOGI(TAG, "Initializing DataLog with LittleFS");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Check if file exists
    FILE* f = fopen(LOG_FILE_PATH, "r");
    if (f == NULL)
    {
        ESP_LOGI(TAG, "Log file not found, creating new one.");
        FILE* f_write = fopen(LOG_FILE_PATH, "w");
        if (f_write == NULL)
        {
            ESP_LOGE(TAG, "Failed to create log file.");
        }
        else
        {
            // Add header
            fprintf(f_write, "timestamp,voltage,current,power\n");
            fclose(f_write);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Log file found.");
        fclose(f);
    }
}

void datalog_add(uint32_t timestamp, float voltage, float current, float power)
{
    char new_line[100];
    int new_line_len = snprintf(new_line, sizeof(new_line), "%lu,%.3f,%.3f,%.3f\n", timestamp, voltage, current, power);

    struct stat st;
    long size = 0;
    if (stat(LOG_FILE_PATH, &st) == 0)
    {
        size = st.st_size;
    }

    if (size + new_line_len <= MAX_LOG_SIZE)
    {
        FILE* f = fopen(LOG_FILE_PATH, "a");
        if (f == NULL)
        {
            ESP_LOGE(TAG, "Failed to open log file for appending.");
            return;
        }
        fputs(new_line, f);
        fclose(f);
    }
    else
    {
        ESP_LOGI(TAG, "Log file is full. Rotating log file.");
        FILE* f_read = fopen(LOG_FILE_PATH, "r");
        if (f_read == NULL)
        {
            ESP_LOGE(TAG, "Could not open log for reading");
            return;
        }

        FILE* f_write = fopen(TEMP_LOG_FILE_PATH, "w");
        if (f_write == NULL)
        {
            ESP_LOGE(TAG, "Could not open temp file for writing");
            fclose(f_read);
            return;
        }

        long size_to_remove = (size + new_line_len) - MAX_LOG_SIZE;
        char line[256];

        // Keep header
        if (fgets(line, sizeof(line), f_read) != NULL)
        {
            fputs(line, f_write);
        }
        else
        {
            ESP_LOGE(TAG, "Could not read header");
            fclose(f_read);
            fclose(f_write);
            return;
        }

        long bytes_skipped = 0;
        while (fgets(line, sizeof(line), f_read) != NULL)
        {
            bytes_skipped += strlen(line);
            if (bytes_skipped >= size_to_remove)
            {
                fputs(line, f_write);
                break;
            }
        }

        while (fgets(line, sizeof(line), f_read) != NULL)
        {
            fputs(line, f_write);
        }

        fputs(new_line, f_write);

        fclose(f_read);
        fclose(f_write);

        if (remove(LOG_FILE_PATH) != 0)
        {
            ESP_LOGE(TAG, "Failed to remove old log file");
        }
        else if (rename(TEMP_LOG_FILE_PATH, LOG_FILE_PATH) != 0)
        {
            ESP_LOGE(TAG, "Failed to rename temp file");
        }
        else
        {
            ESP_LOGI(TAG, "Log file rotated successfully.");
        }
    }
}

const char* datalog_get_path(void)
{
    return LOG_FILE_PATH;
}
