#include "Weather.h"

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "weather";

static weather_cb_t s_cb     = NULL;
static void        *s_user   = NULL;
static char         s_buf[64];
// Guards s_buf + the HTTP client. Weather_FetchOnce is called from two
// places (NetSync's net_task at boot, periodic_task every 30 min) - they
// must NOT overlap or the shared buffer and SPI/WiFi stack will collide.
static SemaphoreHandle_t s_lock = NULL;

// Stream response chunks into a bounded buffer.
static esp_err_t on_http_data(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    int cur = (int)strlen(s_buf);
    int room = (int)sizeof(s_buf) - 1 - cur;
    if (room <= 0) return ESP_OK;
    int copy = evt->data_len < room ? evt->data_len : room;
    memcpy(s_buf + cur, evt->data, copy);
    s_buf[cur + copy] = '\0';
    return ESP_OK;
}

// wttr.in format 3 returns "City: <icon> <temp>C" - keep the suffix only.
static void trim_to_payload(const char *raw, char *out, size_t outlen)
{
    if (!raw || !*raw) { snprintf(out, outlen, "--"); return; }
    const char *colon = strchr(raw, ':');
    const char *src   = colon ? colon + 1 : raw;
    while (*src == ' ') src++;

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s", src);
    size_t n = strlen(tmp);
    while (n && (tmp[n-1] == '\n' || tmp[n-1] == '\r' || tmp[n-1] == ' '))
        tmp[--n] = '\0';
    snprintf(out, outlen, "%s", tmp);
}

void Weather_Init(weather_cb_t cb, void *user)
{
    s_cb   = cb;
    s_user = user;
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
}

bool Weather_FetchOnce(void)
{
    if (s_lock && !xSemaphoreTake(s_lock, pdMS_TO_TICKS(200))) {
        ESP_LOGW(TAG, "fetch skipped: another fetch in progress");
        return false;
    }
    s_buf[0] = '\0';

    esp_http_client_config_t cfg = {};
    cfg.url         = "http://wttr.in/?format=3";
    cfg.timeout_ms  = 8000;
    cfg.event_handler = on_http_data;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status     = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "fetch failed: err=%d status=%d", (int)err, status);
        if (s_lock) xSemaphoreGive(s_lock);
        return false;
    }

    char formatted[64];
    trim_to_payload(s_buf, formatted, sizeof(formatted));
    ESP_LOGI(TAG, "weather: %s", formatted);
    if (s_cb) s_cb(formatted, s_user);
    if (s_lock) xSemaphoreGive(s_lock);
    return true;
}

// Periodic refresh task: re-fetches weather on a fixed cadence so the
// top-right label stays current without needing user interaction.
static void periodic_task(void *arg)
{
    uint32_t period_ms = (uint32_t)(uintptr_t)arg;
    while (1) {
        Weather_FetchOnce();
        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
}

void Weather_StartPeriodic(uint32_t period_ms)
{
    static int started = 0;
    if (started) return;
    started = 1;
    xTaskCreate(periodic_task, "weather", 4096,
                (void *)(uintptr_t)period_ms, 1, NULL);
}