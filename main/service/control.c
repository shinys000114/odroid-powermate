#include "webserver.h"
#include "driver/gpio.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

static const char *TAG = "CONTROL";

// --- GPIO 핀 정의 ---
#define GPIO_12V_SWITCH CONFIG_GPIO_SW_12V
#define GPIO_5V_SWITCH  CONFIG_GPIO_SW_5V
#define GPIO_POWER_TRIGGER CONFIG_GPIO_TRIGGER_POWER
#define GPIO_RESET_TRIGGER CONFIG_GPIO_TRIGGER_RESET

// --- 상태 변수, 뮤텍스 및 타이머 핸들 ---
static bool status_12v_on = false;
static bool status_5v_on = false;
static SemaphoreHandle_t state_mutex;
static esp_timer_handle_t power_trigger_timer;
static esp_timer_handle_t reset_trigger_timer;

/**
 * @brief 타이머 만료 시 GPIO를 다시 HIGH로 설정하는 콜백 함수
 */
static void trigger_off_callback(void* arg)
{
    gpio_num_t gpio_pin = (int) arg;
    gpio_set_level(gpio_pin, 1); // 핀을 다시 HIGH로 복구
    ESP_LOGI(TAG, "GPIO %d trigger finished.", gpio_pin);
}

static void update_gpio_switches()
{
    gpio_set_level(GPIO_12V_SWITCH, status_12v_on);
    gpio_set_level(GPIO_5V_SWITCH, status_5v_on);
    ESP_LOGI(TAG, "Switches updated: 12V=%s, 5V=%s", status_12v_on ? "ON" : "OFF", status_5v_on ? "ON" : "OFF");
}

static esp_err_t control_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    cJSON_AddBoolToObject(root, "load_12v_on", status_12v_on);
    cJSON_AddBoolToObject(root, "load_5v_on", status_5v_on);
    xSemaphoreGive(state_mutex);

    char *json_string = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(root);

    return ESP_OK;
}

static esp_err_t control_post_handler(httpd_req_t *req)
{
    char buf[128];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request content too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    ESP_LOGI(TAG, "Received JSON: %s", buf);

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format");
        return ESP_FAIL;
    }

    bool state_changed = false;
    xSemaphoreTake(state_mutex, portMAX_DELAY);

    cJSON *item_12v = cJSON_GetObjectItem(root, "load_12v_on");
    if (cJSON_IsBool(item_12v)) {
        status_12v_on = cJSON_IsTrue(item_12v);
        state_changed = true;
    }

    cJSON *item_5v = cJSON_GetObjectItem(root, "load_5v_on");
    if (cJSON_IsBool(item_5v)) {
        status_5v_on = cJSON_IsTrue(item_5v);
        state_changed = true;
    }

    if (state_changed) {
        update_gpio_switches();
    }
    xSemaphoreGive(state_mutex);

    cJSON *power_trigger = cJSON_GetObjectItem(root, "power_trigger");
    if (cJSON_IsTrue(power_trigger)) {
        ESP_LOGI(TAG, "Triggering GPIO %d LOW for 3 seconds...", GPIO_POWER_TRIGGER);
        gpio_set_level(GPIO_POWER_TRIGGER, 0);
        esp_timer_stop(power_trigger_timer); // Stop timer if it's already running
        ESP_ERROR_CHECK(esp_timer_start_once(power_trigger_timer, 3000000)); // 3초
    }

    cJSON *reset_trigger = cJSON_GetObjectItem(root, "reset_trigger");
    if (cJSON_IsTrue(reset_trigger)) {
        ESP_LOGI(TAG, "Triggering GPIO %d LOW for 3 seconds...", GPIO_RESET_TRIGGER);
        gpio_set_level(GPIO_RESET_TRIGGER, 0);
        esp_timer_stop(reset_trigger_timer); // Stop timer if it's already running
        ESP_ERROR_CHECK(esp_timer_start_once(reset_trigger_timer, 3000000)); // 3초
    }

    cJSON_Delete(root);

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static void control_module_init(void)
{
    state_mutex = xSemaphoreCreateMutex();

    gpio_config_t switch_conf = {
        .pin_bit_mask = (1ULL << GPIO_12V_SWITCH) | (1ULL << GPIO_5V_SWITCH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&switch_conf);
    update_gpio_switches();

    gpio_config_t trigger_conf = {
        .pin_bit_mask = (1ULL << GPIO_POWER_TRIGGER) | (1ULL << GPIO_RESET_TRIGGER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&trigger_conf);
    gpio_set_level(GPIO_POWER_TRIGGER, 1);
    gpio_set_level(GPIO_RESET_TRIGGER, 1);

    const esp_timer_create_args_t power_timer_args = {
            .callback = &trigger_off_callback,
            .arg = (void*) GPIO_POWER_TRIGGER,
            .name = "power_trigger_off"
    };
    ESP_ERROR_CHECK(esp_timer_create(&power_timer_args, &power_trigger_timer));

    const esp_timer_create_args_t reset_timer_args = {
            .callback = &trigger_off_callback,
            .arg = (void*) GPIO_RESET_TRIGGER,
            .name = "reset_trigger_off"
    };
    ESP_ERROR_CHECK(esp_timer_create(&reset_timer_args, &reset_trigger_timer));

    ESP_LOGI(TAG, "Control module initialized");
}

void register_control_endpoint(httpd_handle_t server)
{
    control_module_init();
    httpd_uri_t get_uri = {
        .uri      = "/api/control",
        .method   = HTTP_GET,
        .handler  = control_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_uri);

    httpd_uri_t post_uri = {
        .uri      = "/api/control",
        .method   = HTTP_POST,
        .handler  = control_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &post_uri);

    ESP_LOGI(TAG, "Registered /api/control endpoints (GET, POST)");
}