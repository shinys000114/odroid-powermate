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

#define UART_NUM UART_NUM_1
#define BUF_SIZE (4096)
#define RD_BUF_SIZE (BUF_SIZE)
#define UART_TX_PIN CONFIG_GPIO_UART_TX
#define UART_RX_PIN CONFIG_GPIO_UART_RX

static const char *TAG = "ws-uart";

static int client_fd = -1;

struct status_message
{
    cJSON *data;
};

QueueHandle_t status_queue;

// Status task
static void status_task(void *arg)
{
    httpd_handle_t server = (httpd_handle_t)arg;
    struct status_message msg;

    while (1) {
        if (xQueueReceive(status_queue, &msg, portMAX_DELAY)) {
            if (client_fd <= 0) continue;

            char *json_string = cJSON_Print(msg.data);
            httpd_ws_frame_t ws_pkt;
            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.payload = (uint8_t *)json_string;
            ws_pkt.len = strlen(json_string);
            ws_pkt.type = HTTPD_WS_TYPE_TEXT;
            esp_err_t err = httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
            free(json_string);
            cJSON_Delete(msg.data);

            if (err != ESP_OK)
            {
                // try close...
                httpd_ws_frame_t close_frame = {
                    .final = true,
                    .fragmented = false,
                    .type = HTTPD_WS_TYPE_CLOSE,
                    .payload = NULL,
                    .len = 0
                };

                httpd_ws_send_frame_async(server, client_fd, &close_frame);
                client_fd = -1;
            }
        }
        vTaskDelay(1);
    }
}

// UART task
static void uart_read_task(void *arg) {
    httpd_handle_t server = (httpd_handle_t)arg;

    uint8_t data[RD_BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, RD_BUF_SIZE, 10 / portTICK_PERIOD_MS);
        if (len > 0 && client_fd != -1) {
            httpd_ws_frame_t ws_pkt;
            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.payload = data;
            ws_pkt.len = len;
            ws_pkt.type = HTTPD_WS_TYPE_BINARY;

            esp_err_t err = httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
            if (err != ESP_OK)
            {
                // try close...
                httpd_ws_frame_t close_frame = {
                    .final = true,
                    .fragmented = false,
                    .type = HTTPD_WS_TYPE_CLOSE,
                    .payload = NULL,
                    .len = 0
                };

                httpd_ws_send_frame_async(server, client_fd, &close_frame);
                client_fd = -1;
            }
        }
        vTaskDelay(1);
    }
}

// 웹소켓 처리 핸들러
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Accept websocket connection");
        client_fd = httpd_req_to_sockfd(req);
        xQueueReset(status_queue);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t buf[BUF_SIZE];
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, BUF_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "웹소켓 프레임 수신 실패");
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
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, ESP_INTR_FLAG_IRAM));

    httpd_uri_t ws = {
        .uri       = "/ws",
        .method    = HTTP_GET,
        .handler   = ws_handler,
        .user_ctx  = NULL,
        .is_websocket = true
    };
    httpd_register_uri_handler(server, &ws);

    status_queue = xQueueCreate(10, sizeof(struct status_message));

    xTaskCreate(uart_read_task, "uart_read_task", 1024*6, server, 8, NULL);
    xTaskCreate(status_task, "status_task", 4096, server, 7, NULL);
}

void push_data_to_ws(cJSON *data)
{
    struct status_message msg;
    msg.data = data;
    if (xQueueSend(status_queue, &msg, 10) != pdPASS)
    {
        ESP_LOGW(TAG, "Queue full");
    }
}

esp_err_t change_baud_rate(int baud_rate)
{
    return uart_set_baudrate(UART_NUM, baud_rate);
}