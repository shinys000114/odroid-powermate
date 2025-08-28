//
// Created by shinys on 25. 8. 18..
//

#include "cJSON.h"
#include "webserver.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nconfig.h"
#include "driver/uart.h"
#include "freertos/semphr.h"

#define UART_NUM UART_NUM_1
#define BUF_SIZE (2048)
#define UART_TX_PIN CONFIG_GPIO_UART_TX
#define UART_RX_PIN CONFIG_GPIO_UART_RX
#define CHUNK_SIZE (1024)

static const char *TAG = "ws-uart";

static int client_fd = -1;
static SemaphoreHandle_t client_fd_mutex;

// Unified message structure for the websocket queue
enum ws_message_type {
    WS_MSG_STATUS,
    WS_MSG_UART
};

struct ws_message
{
    enum ws_message_type type;
    union {
        struct {
            cJSON *data;
        } status;
        struct {
            uint8_t *data;
            size_t len;
        } uart;
    } content;
};

static QueueHandle_t ws_queue;

// Unified task to send data from the queue to the websocket client
static void unified_ws_sender_task(void *arg)
{
    httpd_handle_t server = (httpd_handle_t)arg;
    struct ws_message msg;
    const TickType_t PING_INTERVAL = pdMS_TO_TICKS(5000);

    while (1) {
        if (xQueueReceive(ws_queue, &msg, PING_INTERVAL)) {
            xSemaphoreTake(client_fd_mutex, portMAX_DELAY);
            int fd = client_fd;

            if (fd <= 0) {
                xSemaphoreGive(client_fd_mutex);
                // Free memory if client is not connected
                if (msg.type == WS_MSG_STATUS) {
                    cJSON_Delete(msg.content.status.data);
                } else {
                    free(msg.content.uart.data);
                }
                continue;
            }

            httpd_ws_frame_t ws_pkt = {0};
            esp_err_t err = ESP_FAIL;

            if (msg.type == WS_MSG_STATUS) {
                char *json_string = cJSON_Print(msg.content.status.data);
                cJSON_Delete(msg.content.status.data);

                ws_pkt.payload = (uint8_t *)json_string;
                ws_pkt.len = strlen(json_string);
                ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                err = httpd_ws_send_frame_async(server, fd, &ws_pkt);
                free(json_string);

            } else { // WS_MSG_UART
                ws_pkt.payload = msg.content.uart.data;
                ws_pkt.len = msg.content.uart.len;
                ws_pkt.type = HTTPD_WS_TYPE_BINARY;
                err = httpd_ws_send_frame_async(server, fd, &ws_pkt);
                free(msg.content.uart.data);
            }

            if (err != ESP_OK) {
                ESP_LOGW(TAG, "unified_ws_sender_task: async send failed for fd %d, error: %s", fd, esp_err_to_name(err));
                client_fd = -1;
            }

            xSemaphoreGive(client_fd_mutex);

        } else {
            // Queue receive timed out, send a PING to keep connection alive
            xSemaphoreTake(client_fd_mutex, portMAX_DELAY);
            int fd = client_fd;
            if (fd > 0) {
                httpd_ws_frame_t ping_pkt = {0};
                ping_pkt.type = HTTPD_WS_TYPE_PING;
                ping_pkt.final = true;
                esp_err_t err = httpd_ws_send_frame_async(server, fd, &ping_pkt);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to send PING frame, closing connection for fd %d, error: %s", fd, esp_err_to_name(err));
                    client_fd = -1;
                }
            }
            xSemaphoreGive(client_fd_mutex);
        }
    }
}

static void uart_polling_task(void *arg)
{
    static uint8_t data_buf[BUF_SIZE];
    const TickType_t MIN_POLLING_INTERVAL = pdMS_TO_TICKS(1);
    const TickType_t MAX_POLLING_INTERVAL = pdMS_TO_TICKS(10);
    const TickType_t READ_TIMEOUT = pdMS_TO_TICKS(5);
    
    TickType_t current_interval = MIN_POLLING_INTERVAL;
    int consecutive_empty_polls = 0;
    int cached_client_fd = -1;
    TickType_t last_client_check = 0;
    const TickType_t CLIENT_CHECK_INTERVAL = pdMS_TO_TICKS(100);

    while(1) {
        TickType_t current_time = xTaskGetTickCount();

        if (current_time - last_client_check >= CLIENT_CHECK_INTERVAL) {
            xSemaphoreTake(client_fd_mutex, portMAX_DELAY);
            cached_client_fd = client_fd;
            xSemaphoreGive(client_fd_mutex);
            last_client_check = current_time;
        }

        size_t available_len;
        esp_err_t err = uart_get_buffered_data_len(UART_NUM, &available_len);
        
        if (err != ESP_OK || available_len == 0) {
            consecutive_empty_polls++;
            if (consecutive_empty_polls > 5) {
                current_interval = MAX_POLLING_INTERVAL;
            } else if (consecutive_empty_polls > 2) {
                current_interval = pdMS_TO_TICKS(5);
            }
            
            if (cached_client_fd <= 0) {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }
            
            vTaskDelay(current_interval);
            continue;
        }

        consecutive_empty_polls = 0;
        current_interval = MIN_POLLING_INTERVAL;

        if (cached_client_fd <= 0) {
            uart_flush_input(UART_NUM);
            continue;
        }

        size_t total_processed = 0;
        while (available_len > 0 && total_processed < BUF_SIZE) {
            size_t read_size = (available_len > (BUF_SIZE - total_processed)) ? 
                              (BUF_SIZE - total_processed) : available_len;
            
            int bytes_read = uart_read_bytes(UART_NUM, data_buf + total_processed, 
                                           read_size, READ_TIMEOUT);
            
            if (bytes_read <= 0) {
                break;
            }
            
            total_processed += bytes_read;
            available_len -= bytes_read;

            uart_get_buffered_data_len(UART_NUM, &available_len);
        }

        if (total_processed > 0) {
            size_t offset = 0;
            
            while (offset < total_processed) {
                const size_t chunk_size = (total_processed - offset > CHUNK_SIZE) ?
                                        CHUNK_SIZE : (total_processed - offset);
                
                struct ws_message msg;
                msg.type = WS_MSG_UART;
                msg.content.uart.data = malloc(chunk_size);
                if (!msg.content.uart.data) {
                    ESP_LOGE(TAG, "Failed to allocate memory for uart ws msg");
                    break;
                }
                
                memcpy(msg.content.uart.data, data_buf + offset, chunk_size);
                msg.content.uart.len = chunk_size;

                if (xQueueSend(ws_queue, &msg, 0) != pdPASS) {
                    if (xQueueSend(ws_queue, &msg, pdMS_TO_TICKS(5)) != pdPASS) {
                        ESP_LOGW(TAG, "ws sender queue full, dropping %zu bytes", chunk_size);
                        free(msg.content.uart.data);
                    }
                }
                
                offset += chunk_size;
            }
        }

        if (available_len > 0) {
            vTaskDelay(MIN_POLLING_INTERVAL);
        } else {
            vTaskDelay(current_interval);
        }
    }
    
    vTaskDelete(NULL);
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        xSemaphoreTake(client_fd_mutex, portMAX_DELAY);
        if (client_fd > 0) {
            // A client is already connected. Reject the new connection.
            ESP_LOGW(TAG, "Another client tried to connect, but a session is already active. Rejecting.");
            xSemaphoreGive(client_fd_mutex);
            httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Another client is already connected");
            return ESP_FAIL;
        }

        // No client is connected. Accept the new one.
        int new_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "Accepting new websocket connection: %d", new_fd);
        client_fd = new_fd;
        xSemaphoreGive(client_fd_mutex);

        // Reset queue and flush UART buffer for the new session
        xQueueReset(ws_queue);
        uart_flush_input(UART_NUM);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = {0};
    uint8_t buf[BUF_SIZE];
    ws_pkt.payload = buf;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, BUF_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "httpd_ws_recv_frame failed with error: %s", esp_err_to_name(ret));
        xSemaphoreTake(client_fd_mutex, portMAX_DELAY);
        if (httpd_req_to_sockfd(req) == client_fd) {
            client_fd = -1;
        }
        xSemaphoreGive(client_fd_mutex);
        return ret;
    }

    uart_write_bytes(UART_NUM, (const char *)ws_pkt.payload, ws_pkt.len);

    return ESP_OK;
}

void register_ws_endpoint(httpd_handle_t server)
{
    size_t baud_rate_len;

    nconfig_get_str_len(UART_BAUD_RATE, &baud_rate_len);
    char buf[baud_rate_len];
    nconfig_read(UART_BAUD_RATE, buf, baud_rate_len);

    uart_config_t uart_config = {
        .baud_rate = strtol(buf, NULL, 10),
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        // .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0));

    httpd_uri_t ws = {
        .uri       = "/ws",
        .method    = HTTP_GET,
        .handler   = ws_handler,
        .user_ctx  = NULL,
        .is_websocket = true
    };
    httpd_register_uri_handler(server, &ws);

    client_fd_mutex = xSemaphoreCreateMutex();
    ws_queue = xQueueCreate(10, sizeof(struct ws_message)); // Combined queue

    xTaskCreate(uart_polling_task, "uart_polling_task", 1024*4, NULL, 8, NULL);
    xTaskCreate(unified_ws_sender_task, "ws_sender_task", 1024*6, server, 9, NULL);
}

void push_data_to_ws(cJSON *data)
{
    struct ws_message msg;
    msg.type = WS_MSG_STATUS;
    msg.content.status.data = data;
    if (xQueueSend(ws_queue, &msg, pdMS_TO_TICKS(10)) != pdPASS)
    {
        ESP_LOGW(TAG, "WS queue full, dropping status message");
        cJSON_Delete(data);
    }
}

esp_err_t change_baud_rate(int baud_rate)
{
    return uart_set_baudrate(UART_NUM, baud_rate);
}
