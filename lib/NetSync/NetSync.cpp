#include "NetSync.h"

#include <time.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "Weather.h"

#define WIFI_SSID      "Xiaomi_A520"
#define WIFI_PASS      "wxk@1128"
#define WIFI_MAX_RETRY 15

static const char *TAG = "net";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0
static int s_retry_num = 0;

// --- WiFi ---

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "wifi retry %d/%d", s_retry_num, WIFI_MAX_RETRY);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&e->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static bool wifi_connect_blocking(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t any_id, got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, &any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, &got_ip));

    wifi_config_t wc = {};
    strncpy((char *)wc.sta.ssid,     WIFI_SSID, sizeof(wc.sta.ssid));
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE, pdMS_TO_TICKS(20000));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

// --- NTP ---

static bool ntp_sync_blocking(int timeout_sec)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "cn.pool.ntp.org");
    esp_sntp_setservername(2, "time.windows.com");
    esp_sntp_init();

    int waited_ms = 0;
    while (waited_ms < timeout_sec * 1000) {
        time_t now; struct tm ti;
        time(&now); localtime_r(&now, &ti);
        if (ti.tm_year >= (2024 - 1900)) {
            ESP_LOGI(TAG, "ntp ok: %04d-%02d-%02d %02d:%02d:%02d",
                     ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                     ti.tm_hour, ti.tm_min, ti.tm_sec);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        waited_ms += 500;
    }
    return false;
}

// --- Weather bridge ---

static void on_weather(const char *text, void *user)
{
    (void)text;
    (void)user;
    // The clock face no longer renders weather on-screen, but we still
    // pull it from wttr.in so the data is available for future use.
    ESP_LOGI(TAG, "weather fetched: %s", text);
}

// --- Entry point ---

static void net_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "net task start");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    if (!wifi_connect_blocking()) {
        ESP_LOGW(TAG, "wifi connect failed; clock keeps fallback time");
    } else {
        ntp_sync_blocking(30);
        Weather_FetchOnce();
    }
    ESP_LOGI(TAG, "net task done");
    vTaskDelete(NULL);
}

void NetSync_StartBackground(void)
{
    static int started = 0;
    if (started) return;
    started = 1;
    Weather_Init(on_weather, NULL);
    xTaskCreate(net_task, "net_sync", 8192, NULL, 1, NULL);
}