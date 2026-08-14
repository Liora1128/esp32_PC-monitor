#include "NetSync.h"
#include "wifi_provision.h"


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

// ============================================================
// 配置
// ============================================================

#define WIFI_MAX_RETRY       15
#define WIFI_CONNECT_TIMEOUT 20000

#define UDP_PORT 9999
#define RECV_BUF_SIZE 1024

static const char *TAG = "net";

// ============================================================
// 网络状态
// ============================================================

static volatile NetSync_State s_net_state =
    NETSYNC_STATE_STARTING;

NetSync_State NetSync_GetState(void)
{
    return s_net_state;
}

void NetSync_SetState(NetSync_State state)
{
    s_net_state = state;
}

// ============================================================
// 配网 UI 信息
// ============================================================

static char s_provision_ssid[64] = {0};

static char s_provision_error[128] = {0};

void NetSync_SetProvisionSSID(
    const char *ssid)
{
    if (!ssid)
    {
        s_provision_ssid[0] = '\0';
        return;
    }

    strncpy(
        s_provision_ssid,
        ssid,
        sizeof(s_provision_ssid) - 1
    );

    s_provision_ssid[
        sizeof(s_provision_ssid) - 1
    ] = '\0';
}

const char *NetSync_GetProvisionSSID(void)
{
    return s_provision_ssid;
}

void NetSync_SetProvisionError(
    const char *msg)
{
    if (!msg)
    {
        s_provision_error[0] = '\0';
        return;
    }

    strncpy(
        s_provision_error,
        msg,
        sizeof(s_provision_error) - 1
    );

    s_provision_error[
        sizeof(s_provision_error) - 1
    ] = '\0';
}

const char *NetSync_GetProvisionError(void)
{
    return s_provision_error;
}

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0

static int s_retry_num = 0;

static NetSync_Data s_data;



// ============================================================
// Wi-Fi 正常模式连接
// ============================================================

static void wifi_event_handler(
    void *arg,
    esp_event_base_t base,
    int32_t id,
    void *data)
{
    (void)arg;

    if (base == WIFI_EVENT &&
        id == WIFI_EVENT_STA_START) {

        ESP_LOGI(
            TAG,
            "STA started, connecting..."
        );

        esp_wifi_connect();
    }

    else if (
        base == WIFI_EVENT &&
        id == WIFI_EVENT_STA_DISCONNECTED) {

        wifi_event_sta_disconnected_t *e =
            (wifi_event_sta_disconnected_t *)data;

        int reason =
            e ? e->reason : -1;

        ESP_LOGW(
            TAG,
            "Wi-Fi disconnected, reason=%d",
            reason
        );

        if (s_retry_num < WIFI_MAX_RETRY) {

            s_retry_num++;

            esp_wifi_connect();

            ESP_LOGW(
                TAG,
                "Wi-Fi retry %d/%d",
                s_retry_num,
                WIFI_MAX_RETRY
            );
        }
        else {

            ESP_LOGE(
                TAG,
                "Wi-Fi retry limit reached"
            );
        }
    }

    else if (
        base == IP_EVENT &&
        id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t *e =
            (ip_event_got_ip_t *)data;

        ESP_LOGI(
            TAG,
            "Wi-Fi connected, IP="
            IPSTR,
            IP2STR(&e->ip_info.ip)
        );

        s_retry_num = 0;

        xEventGroupSetBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT
        );
    }
}

// ============================================================
// mDNS
// ============================================================

static bool start_mdns()
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) 
    {
        ESP_LOGE(
            TAG,
            "mdns_init failed: %s",
            esp_err_to_name(err)
        );
        return false;
    }

    err = mdns_hostname_set("pcmonitor");

    if (err != ESP_OK) 
    {
        ESP_LOGE(
            TAG,
            "mdns_hostname_set failed: %s",
            esp_err_to_name(err)
        );
        mdns_free();
        return false;
    }

    // 注册 UDP 9999 服务
    //
    // _pcmonitor._udp.local
    //
    // 以后树莓派除了可以解析：
    //
    // pcmonitor.local
    //
    // 也可以通过服务发现找到 9999。
    err = mdns_service_add(
        "PCMonitor",
        "_pcmonitor",
        "_udp",
        UDP_PORT,
        NULL,
        0
    );

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "mdns_service_add failed: %s",
            esp_err_to_name(err)
        );
    }

    ESP_LOGI(
        TAG,
        "mDNS started: pcmonitor.local, UDP %d",
        UDP_PORT
    );

    return true;


}

// ============================================================
// 连接已保存 Wi-Fi
// ============================================================

static bool wifi_connect_blocking(
    const char *ssid,
    const char *pass)
{
    ESP_LOGI(
        TAG,
        "connecting saved Wi-Fi: %s",
        ssid
    );

    s_wifi_event_group =
        xEventGroupCreate();

    if (!s_wifi_event_group) {

        ESP_LOGE(
            TAG,
            "event group create failed"
        );

        return false;
    }

    // --------------------------------------------------------
    // 网络栈
    // --------------------------------------------------------

    esp_err_t err =
        esp_netif_init();

    if (err != ESP_OK &&
        err != ESP_ERR_INVALID_STATE) {

        ESP_LOGE(
            TAG,
            "esp_netif_init failed: %s",
            esp_err_to_name(err)
        );

        return false;
    }

    static bool event_loop_created = false;

    if (!event_loop_created) {

        err =
            esp_event_loop_create_default();

        if (err != ESP_OK &&
            err != ESP_ERR_INVALID_STATE) {

            ESP_LOGE(
                TAG,
                "event loop create failed: %s",
                esp_err_to_name(err)
            );

            return false;
        }

        event_loop_created = true;
    }

    esp_netif_t *sta_netif =
        esp_netif_create_default_wifi_sta();

    if (!sta_netif) {

        ESP_LOGE(
            TAG,
            "create STA netif failed"
        );

        return false;
    }

    // --------------------------------------------------------
    // Wi-Fi driver
    // --------------------------------------------------------

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    err =
        esp_wifi_init(&cfg);

    if (err != ESP_OK &&
        err != ESP_ERR_INVALID_STATE) {

        ESP_LOGE(
            TAG,
            "esp_wifi_init failed: %s",
            esp_err_to_name(err)
        );

        return false;
    }

    // --------------------------------------------------------
    // 注册事件
    // --------------------------------------------------------

    esp_event_handler_instance_t any_id = NULL;
    esp_event_handler_instance_t got_ip = NULL;

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            &any_id
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            &got_ip
        )
    );

    // --------------------------------------------------------
    // STA 配置
    // --------------------------------------------------------

    wifi_config_t wc = {};

    strncpy(
        (char *)wc.sta.ssid,
        ssid,
        sizeof(wc.sta.ssid) - 1
    );

    if (pass) {

        strncpy(
            (char *)wc.sta.password,
            pass,
            sizeof(wc.sta.password) - 1
        );
    }

    // 不强制 WPA2
    wc.sta.pmf_cfg.capable = true;
    wc.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(
            WIFI_MODE_STA
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wc
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_ps(
            WIFI_PS_NONE
        )
    );

    // --------------------------------------------------------
    // 等待 GOT_IP
    // --------------------------------------------------------

    s_retry_num = 0;

    EventBits_t bits =
        xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(
                WIFI_CONNECT_TIMEOUT
            )
        );

    bool ok =
        (bits & WIFI_CONNECTED_BIT)
        != 0;

    esp_event_handler_instance_unregister(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        any_id
    );

    esp_event_handler_instance_unregister(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        got_ip
    );

    if (ok) {

        ESP_LOGI(
            TAG,
            "saved Wi-Fi connection successful"
        );
    }
    else {

        ESP_LOGE(
            TAG,
            "saved Wi-Fi connection timeout/failure"
        );

        esp_wifi_disconnect();
        esp_wifi_stop();
    }

    vEventGroupDelete(
        s_wifi_event_group
    );

    s_wifi_event_group = NULL;

    return ok;
}

// ============================================================
// JSON 解析
// ============================================================

static int json_int(
    const char *json,
    const char *key,
    int def)
{
    char k[32];

    snprintf(
        k,
        sizeof(k),
        "\"%s\":",
        key
    );

    const char *p =
        strstr(json, k);

    if (!p)
        return def;

    p += strlen(k);

    while (*p == ' ')
        p++;

    return atoi(p);
}

static float json_float(
    const char *json,
    const char *key,
    float def)
{
    char k[32];

    snprintf(
        k,
        sizeof(k),
        "\"%s\":",
        key
    );

    const char *p =
        strstr(json, k);

    if (!p)
        return def;

    p += strlen(k);

    while (*p == ' ')
        p++;

    return (float)atof(p);
}

static void json_str(
    const char *json,
    const char *key,
    char *out,
    size_t n)
{
    if (!out || n == 0)
        return;

    char k[64];

    snprintf(
        k,
        sizeof(k),
        "\"%s\":\"",
        key
    );

    const char *p =
        strstr(json, k);

    if (!p) {
        out[0] = '\0';
        return;
    }

    p += strlen(k);

    size_t i = 0;

    while (
        *p &&
        *p != '"' &&
        i + 1 < n
    ) {

        out[i++] =
            *p++;
    }

    out[i] = '\0';
}

// ============================================================
// 解析 PC 发来的监控数据
// ============================================================

static void parse_json_line(
    const char *json)
{
    NetSync_Data *d =
        &s_data;

    // ========================================================
    // 检查本次数据包是否真的包含对应字段
    // ========================================================

    d->has_cpu_temp =
        strstr(
            json,
            "\"cpu_t\":"
        ) != NULL;

    d->has_uptime =
        strstr(
            json,
            "\"uptime\":"
        ) != NULL;

    d->cpu =
        (uint8_t)json_int(
            json,
            "cpu",
            d->cpu
        );

    d->cpu_temp =
        (uint8_t)json_int(
            json,
            "cpu_t",
            d->cpu_temp
        );

    int f100 =
        json_int(
            json,
            "freq100",
            -1
        );

    if (f100 <= 0) {

        f100 =
            (int)(
                json_float(
                    json,
                    "freq",
                    0.0f
                ) * 100.0f
                + 0.5f
            );
    }

    d->freq_mhz =
        (uint16_t)f100;

    d->ram_pct =
        (uint8_t)json_int(
            json,
            "ram",
            d->ram_pct
        );

    d->ram_used_gb =
        json_float(
            json,
            "ram_u",
            d->ram_used_gb
        );

    d->ram_total_gb =
        json_float(
            json,
            "ram_t",
            d->ram_total_gb
        );

    d->swap_pct =
        (uint8_t)json_int(
            json,
            "swap",
            d->swap_pct
        );

    d->up_bps =
        (uint32_t)json_int(
            json,
            "up",
            d->up_bps
        );

    d->down_bps =
        (uint32_t)json_int(
            json,
            "down",
            d->down_bps
        );

    d->up_total =
        (uint64_t)json_int(
            json,
            "up_t",
            (int)d->up_total
        );

    d->down_total =
        (uint64_t)json_int(
            json,
            "dn_t",
            (int)d->down_total
        );

    d->gpu_load =
        (uint8_t)json_int(
            json,
            "gpu",
            d->gpu_load
        );

    d->gpu_mem_pct =
        (uint8_t)json_int(
            json,
            "gpu_m",
            d->gpu_mem_pct
        );

    d->gpu_temp =
        (uint8_t)json_int(
            json,
            "gpu_t",
            d->gpu_temp
        );

    d->disk_read_kbs =
        (uint16_t)json_int(
            json,
            "d_r",
            d->disk_read_kbs
        );

    d->disk_write_kbs =
        (uint16_t)json_int(
            json,
            "d_w",
            d->disk_write_kbs
        );

    d->uptime_sec =
        (uint32_t)json_int(
            json,
            "uptime",
            d->uptime_sec
        );

    json_str(
        json,
        "nic",
        d->nic_name,
        sizeof(d->nic_name)
    );

    const char *p =
        strstr(
            json,
            "\"cores\":["
        );

    if (p) {

        p += strlen(
            "\"cores\":["
        );

        for (int i = 0; i < 8; i++) {

            while (*p == ' ')
                p++;

            if (*p == ']')
                break;

            d->cores[i] =
                (uint8_t)atoi(p);

            while (
                *p &&
                *p != ',' &&
                *p != ']'
            ) {
                p++;
            }

            if (*p == ',')
                p++;
        }
    }

    d->valid = 1;
}

// ============================================================
// UDP 9999：PC -> ESP32
// ============================================================

static void udp_recv_task(
    void *arg)
{
    (void)arg;

    int sock =
        socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP
        );

    if (sock < 0)
    {
        ESP_LOGE(
            TAG,
            "UDP socket failed"
        );

        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr;

    memset(
        &addr,
        0,
        sizeof(addr)
    );

    addr.sin_family =
        AF_INET;

    addr.sin_port =
        htons(UDP_PORT);

    addr.sin_addr.s_addr =
        htonl(INADDR_ANY);

    if (
        bind(
            sock,
            (struct sockaddr *)&addr,
            sizeof(addr)
        ) < 0
    )
    {
        ESP_LOGE(
            TAG,
            "UDP bind failed on port %d",
            UDP_PORT
        );

        close(sock);

        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(
        TAG,
        "listening UDP %d",
        UDP_PORT
    );

    char buf[
        RECV_BUF_SIZE
    ];

    while (1)
    {
        struct sockaddr_in from;

        socklen_t fl =
            sizeof(from);

        int n =
            recvfrom(
                sock,
                buf,
                sizeof(buf) - 1,
                0,
                (struct sockaddr *)&from,
                &fl
            );

        if (n <= 0)
            continue;

        buf[n] = '\0';


        // ====================================================
        // 原来的 JSON 解析逻辑
        // ====================================================

        char *line =
            buf;

        while (
            line &&
            *line
        )
        {
            char *eol =
                strchr(
                    line,
                    '\n'
                );

            if (eol)
                *eol = '\0';

            if (*line)
            {
                parse_json_line(
                    line
                );
            }

            if (!eol)
                break;

            line =
                eol + 1;
        }
    }
}


// ============================================================
// 总入口
// ============================================================

void NetSync_StartBackground(void)
{
    s_net_state =
        NETSYNC_STATE_STARTING;

    ESP_LOGI(
        TAG,
        "network subsystem start"
    );

    // --------------------------------------------------------
    // 初始化 NVS
    // --------------------------------------------------------

    esp_err_t ret =
        nvs_flash_init();

    if (
        ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {

        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ESP_ERROR_CHECK(
            nvs_flash_init()
        );
    }

    // --------------------------------------------------------
    // 读取保存的 Wi-Fi
    // --------------------------------------------------------

    char ssid[64] = {0};
    char pass[64] = {0};

    if (
        !WifiProvision_Load(
            ssid,
            sizeof(ssid),
            pass,
            sizeof(pass)
        )
    ) {

        ESP_LOGW(
            TAG,
            "no saved Wi-Fi credentials"
        );

        ESP_LOGI(
            TAG,
            "entering provisioning mode"
        );

        s_net_state =
            NETSYNC_STATE_PROVISIONING;

        // ----------------------------------------------------
        // 这里会一直运行配网。
        // 配网成功后才返回。
        // ----------------------------------------------------

        bool provision_ok =
            WifiProvision_StartAP();

        if (!provision_ok)
        {
            ESP_LOGE(
                TAG,
                "Wi-Fi provisioning failed"
            );

            return;
        }

        // ----------------------------------------------------
        // 配网成功
        //
        // 此时：
        //   NVS 已保存
        //   STA 已连接
        //   AP 已关闭
        // ----------------------------------------------------

        ESP_LOGI(
            TAG,
            "provisioning successful"
        );

        s_net_state =
            NETSYNC_STATE_PROVISION_SUCCESS;

        vTaskDelay(
            pdMS_TO_TICKS(1500)
        );

        s_net_state =
            NETSYNC_STATE_READY;
    }
    else
    {
        // ----------------------------------------------------
        // 已经有保存的 Wi-Fi
        // ----------------------------------------------------

        ESP_LOGI(
            TAG,
            "saved Wi-Fi found: %s",
            ssid
        );

        s_net_state =
            NETSYNC_STATE_CONNECTING;

        // ----------------------------------------------------
        // 正常连接
        // ----------------------------------------------------

        if (
            !wifi_connect_blocking(
                ssid,
                pass
            )
        ) {

            ESP_LOGW(
                TAG,
                "saved Wi-Fi failed"
            );

            WifiProvision_ClearCredentials();

            ESP_LOGW(
                TAG,
                "credentials cleared, rebooting into provisioning mode"
            );

            vTaskDelay(
                pdMS_TO_TICKS(500)
            );

            esp_restart();

            return;
        }

        // ----------------------------------------------------
        // 已保存 Wi-Fi 正常连接成功
        //
        // 不立即进入 READY。
        // 先给屏幕一个完整的"连接成功"过渡。
        // ----------------------------------------------------

        s_net_state =
            NETSYNC_STATE_CONNECT_SUCCESS;

        vTaskDelay(
            pdMS_TO_TICKS(1200)
        );
    }

// --------------------------------------------------------
// Wi-Fi 已经连接成功
// 现在发布 mDNS
// --------------------------------------------------------


    s_net_state =
        NETSYNC_STATE_READY;

    
    if (!start_mdns())
    {
        ESP_LOGE(
            TAG,
            "mDNS start failed"
        );
    }

    ESP_LOGI(
        TAG,
        "starting monitor UDP services"
    );

    if (
        xTaskCreate(
            udp_recv_task,
            "netsync_udp",
            4096,
            NULL,
            5,
            NULL
        ) != pdPASS
    ) {

        ESP_LOGE(
            TAG,
            "failed to start UDP receiver"
        );
    }


    ESP_LOGI(
        TAG,
        "network subsystem ready"
    );
}

// ============================================================
// 对外提供监控数据
// ============================================================

const NetSync_Data *NetSync_Get(void)
{
    return &s_data;
}