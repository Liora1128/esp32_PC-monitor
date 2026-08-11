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

#include "lwip/sockets.h"
#include "lwip/inet.h"

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

// --- UDP JSON receiver (PC -> ESP32, port 9999) ---

#define UDP_PORT        9999
#define RECV_BUF_SIZE   1024

static NetSync_Data s_data;

static int json_int(const char *json, const char *key, int def)
{
    char k[32];
    snprintf(k, sizeof(k), "\"%s\":", key);
    const char *p = strstr(json, k);
    if (!p) return def;
    p += strlen(k);
    while (*p == ' ') p++;
    return atoi(p);
}

static float json_float(const char *json, const char *key, float def)
{
    char k[32];
    snprintf(k, sizeof(k), "\"%s\":", key);
    const char *p = strstr(json, k);
    if (!p) return def;
    p += strlen(k);
    while (*p == ' ') p++;
    return (float)atof(p);
}

static void json_str(const char *json, const char *key, char *out, size_t n)
{
    char k[32];
    snprintf(k, sizeof(k), "\"%s\":\"", key);
    const char *p = strstr(json, k);
    if (!p) { if (n) out[0] = 0; return; }
    p += strlen(k);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < n) out[i++] = *p++;
    out[i] = 0;
}

static void parse_json_line(const char *json)
{
    NetSync_Data *d = &s_data;

    d->cpu       = (uint8_t)  json_int(json, "cpu",    d->cpu);
    d->cpu_temp  = (uint8_t)  json_int(json, "cpu_t",  d->cpu_temp);
    // "freq100" = GHz * 100 (e.g. 2.72 GHz -> 272 -> display "2.72").
    int f100 = json_int(json, "freq100", -1);
    if (f100 <= 0) {
        // Legacy fallback: "freq" was raw GHz -> convert to GHz*100.
        f100 = (json_float(json, "freq", 0.0f) * 100.0f + 0.5f);
    }
    d->freq_mhz  = (uint16_t) f100;
    d->ram_pct   = (uint8_t)  json_int(json, "ram",    d->ram_pct);
    d->ram_used_gb  = json_float(json, "ram_u", d->ram_used_gb);
    d->ram_total_gb = json_float(json, "ram_t", d->ram_total_gb);
    d->swap_pct  = (uint8_t)  json_int(json, "swap",   d->swap_pct);
    d->up_bps    = (uint32_t) json_int(json, "up",     d->up_bps);
    d->down_bps  = (uint32_t) json_int(json, "down",   d->down_bps);
    d->up_total  = (uint64_t) json_int(json, "up_t",   (int)d->up_total);
    d->down_total= (uint64_t) json_int(json, "dn_t",   (int)d->down_total);
    d->gpu_load  = (uint8_t)  json_int(json, "gpu",    d->gpu_load);
    d->gpu_mem_pct=(uint8_t)  json_int(json, "gpu_m",  d->gpu_mem_pct);
    d->gpu_temp  = (uint8_t)  json_int(json, "gpu_t",  d->gpu_temp);
    d->disk_read_kbs  = (uint16_t) json_int(json, "d_r", d->disk_read_kbs);
    d->disk_write_kbs = (uint16_t) json_int(json, "d_w", d->disk_write_kbs);
    d->uptime_sec= (uint32_t) json_int(json, "uptime", d->uptime_sec);
    json_str(json, "nic", d->nic_name, sizeof(d->nic_name));

    // cores array, up to 8
    const char *p = strstr(json, "\"cores\":[");
    if (p) {
        p += strlen("\"cores\":[");
        for (int i = 0; i < 8; i++) {
            while (*p == ' ') p++;
            if (*p == ']') break;
            d->cores[i] = (uint8_t)atoi(p);
            while (*p && *p != ',' && *p != ']') p++;
            if (*p == ',') p++;
        }
    }
    d->valid = 1;
}

static void udp_recv_task(void *arg)
{
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "udp socket() failed");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(UDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "udp bind failed on port %d", UDP_PORT);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "listening on UDP %d (PC -> ESP32 JSON)", UDP_PORT);

    char buf[RECV_BUF_SIZE];
    while (1) {
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        int n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                         (struct sockaddr *)&from, &fl);
        if (n <= 0) continue;
        buf[n] = 0;

        char *line = buf;
        while (line && *line) {
            char *eol = strchr(line, '\n');
            if (eol) *eol = 0;
            parse_json_line(line);
            if (!eol) break;
            line = eol + 1;
        }
    }
}

// --- Hello broadcaster: lets the PC learn our IP via UDP 9998 ---

#define HELLO_PORT     9998
#define HELLO_PERIOD   5000
#define HELLO_MAGIC    "ESPSTATS_HELLO"

static void hello_task(void *arg)
{
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { vTaskDelete(NULL); return; }
    int bc = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(HELLO_PORT);
    dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    int seq = 0;
    char msg[96];
    while (1) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"),
                                   &ip) == ESP_OK && ip.ip.addr != 0) {
            int n = snprintf(msg, sizeof(msg),
                             "%s " IPSTR " %d",
                             HELLO_MAGIC, IP2STR(&ip.ip), seq++);
            sendto(sock, msg, n, 0, (struct sockaddr *)&dst, sizeof(dst));
        }
        vTaskDelay(pdMS_TO_TICKS(HELLO_PERIOD));
    }
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

    bool wifi_ok = wifi_connect_blocking();
    if (!wifi_ok) {
        ESP_LOGW(TAG, "wifi connect failed; clock keeps fallback time");
    } else {
        ntp_sync_blocking(30);
        Weather_FetchOnce();
        // Start the PC JSON receiver alongside weather/etc.
        xTaskCreate(udp_recv_task, "netsync_udp", 4096, NULL, 5, NULL);
        // Broadcast hello every 5s so the PC can find our IP.
        xTaskCreate(hello_task, "netsync_hello", 2048, NULL, 4, NULL);
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

const NetSync_Data *NetSync_Get(void)
{
    return &s_data;
}