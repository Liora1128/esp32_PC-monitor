#include "wifi_provision.h"
#include "ProvisionUI.h"
#include "NetSync.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static const char *TAG = "wifi_prov";

#define PROV_AP_SSID "PCMonitor-Setup"
#define PROV_AP_IP "192.168.4.1"
#define PROV_DNS_PORT 53
#define PROV_HTTP_PORT 80

#define VERIFY_TIMEOUT_MS 30000
#define STATUS_POLL_MS 500

#define MAX_SCAN_AP 32
static wifi_ap_record_t s_scan_records[MAX_SCAN_AP];
static uint16_t s_scan_count = 0;

// ============================================================
// 前端页面
// ============================================================

static const char INDEX_HTML[] =
    "<!doctype html>"
    "<html lang='en'>"
    "<head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>"
    "<meta name='theme-color' content='#f5f7fb'>"
    "<title>PCMonitor Wi-Fi Setup</title>"

    "<style>"

    "*{box-sizing:border-box}"

    "body{"
    "margin:0;"
    "min-height:100vh;"
    "padding:20px;"
    "display:flex;"
    "align-items:center;"
    "justify-content:center;"
    "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Arial,sans-serif;"
    "background:#f3f6fb;"
    "color:#172033;"
    "}"

    ".card{"
    "width:100%;"
    "max-width:430px;"
    "background:#fff;"
    "border-radius:22px;"
    "padding:28px;"
    "box-shadow:0 18px 50px rgba(22,34,58,.10);"
    "}"

    ".brand{"
    "font-size:14px;"
    "font-weight:700;"
    "letter-spacing:.08em;"
    "text-transform:uppercase;"
    "color:#2563eb;"
    "margin-bottom:10px;"
    "}"

    "h1{"
    "margin:0;"
    "font-size:28px;"
    "line-height:1.2;"
    "}"

    ".subtitle{"
    "margin:9px 0 26px;"
    "font-size:14px;"
    "line-height:1.5;"
    "color:#697386;"
    "}"

    ".label{"
    "display:block;"
    "margin:0 0 8px;"
    "font-size:13px;"
    "font-weight:600;"
    "color:#374151;"
    "}"

    ".field{"
    "margin-bottom:18px;"
    "}"

    "select,input{"
    "width:100%;"
    "height:50px;"
    "padding:0 14px;"
    "border:1px solid #d7deea;"
    "border-radius:12px;"
    "background:#fff;"
    "color:#111827;"
    "font-size:16px;"
    "outline:none;"
    "}"

    "select:focus,input:focus{"
    "border-color:#2563eb;"
    "box-shadow:0 0 0 3px rgba(37,99,235,.12);"
    "}"

    ".password-wrap{"
    "position:relative;"
    "}"

    ".password-wrap input{"
    "padding-right:52px;"
    "}"

    ".eye{"
    "position:absolute;"
    "top:50%;"
    "right:8px;"
    "transform:translateY(-50%);"
    "width:38px;"
    "height:38px;"
    "border:0;"
    "border-radius:9px;"
    "background:transparent;"
    "color:#667085;"
    "cursor:pointer;"
    "font-size:18px;"
    "}"

    ".signal{"
    "margin-top:8px;"
    "font-size:12px;"
    "color:#6b7280;"
    "min-height:18px;"
    "}"

    "button[type=submit]{"
    "width:100%;"
    "height:50px;"
    "border:0;"
    "border-radius:12px;"
    "background:#2563eb;"
    "color:#fff;"
    "font-size:16px;"
    "font-weight:700;"
    "cursor:pointer;"
    "transition:.15s;"
    "}"

    "button[type=submit]:active{"
    "transform:scale(.99);"
    "}"

    "button[type=submit]:disabled{"
    "background:#9aa7ba;"
    "cursor:not-allowed;"
    "}"

    ".msg{"
    "display:none;"
    "margin-top:16px;"
    "padding:13px 14px;"
    "border-radius:12px;"
    "font-size:14px;"
    "line-height:1.45;"
    "}"

    ".msg.info{"
    "display:block;"
    "background:#eff6ff;"
    "color:#1d4ed8;"
    "}"

    ".msg.error{"
    "display:block;"
    "background:#fef2f2;"
    "color:#b91c1c;"
    "}"

    ".msg.success{"
    "display:block;"
    "background:#ecfdf3;"
    "color:#047857;"
    "}"

    ".spinner{"
    "display:inline-block;"
    "width:15px;"
    "height:15px;"
    "margin-right:8px;"
    "border:2px solid rgba(255,255,255,.45);"
    "border-top-color:#fff;"
    "border-radius:50%;"
    "vertical-align:-2px;"
    "animation:spin .7s linear infinite;"
    "}"

    "@keyframes spin{"
    "to{transform:rotate(360deg)}"
    "}"

    ".footer{"
    "margin-top:18px;"
    "text-align:center;"
    "font-size:12px;"
    "color:#98a2b3;"
    "}"

    "</style>"
    "</head>"

    "<body>"

    "<div class='card'>"

    "<div class='brand'>PCMonitor</div>"

    "<h1>Wi-Fi Setup</h1>"

    "<div class='subtitle'>"
    "Connect your monitor to your local Wi-Fi network."
    "</div>"

    "<form id='form'>"

    "<div class='field'>"
    "<label class='label' for='ssid'>Wi-Fi network</label>"
    "<select id='ssid'>"
    "<option value=''>Scanning nearby networks...</option>"
    "</select>"
    "<div id='signal' class='signal'></div>"
    "</div>"

    "<div class='field'>"
    "<label class='label' for='pass'>Password</label>"
    "<div class='password-wrap'>"
    "<input id='pass' type='password' autocomplete='off' "
    "placeholder='Enter Wi-Fi password'>"
    "<button id='eye' class='eye' type='button' aria-label='Show password'>"
    "&#128065;"
    "</button>"
    "</div>"
    "</div>"

    "<button id='go' type='submit'>Connect</button>"

    "<div id='msg' class='msg'></div>"

    "</form>"

    "<div class='footer'>"
    "Your Wi-Fi password is stored locally on the device."
    "</div>"

    "</div>"

    "<script>"

    "const form=document.getElementById('form');"
    "const ssid=document.getElementById('ssid');"
    "const pass=document.getElementById('pass');"
    "const go=document.getElementById('go');"
    "const msg=document.getElementById('msg');"
    "const signal=document.getElementById('signal');"
    "const eye=document.getElementById('eye');"

    "let statusTimer=null;"
    "let reconnectTimer=null;"

    "function setBusy(busy,text){"
    "  go.disabled=busy;"
    "  go.innerHTML=busy"
    "    ? '<span class=\"spinner\"></span>'+text"
    "    : 'Connect';"
    "}"

    "function showMsg(text,type){"
    "  msg.className='msg '+type;"
    "  msg.textContent=text;"
    "  msg.style.display='block';"
    "}"

    "function clearMsg(){"
    "  msg.textContent='';"
    "  msg.style.display='none';"
    "  msg.className='msg';"
    "}"

    "function updateSignal(){"
    "  const opt=ssid.options[ssid.selectedIndex];"
    "  if(!opt || !opt.dataset.rssi){"
    "    signal.textContent='';"
    "    return;"
    "  }"

    "  const r=Number(opt.dataset.rssi);"

    "  if(r>=-55) signal.textContent='Excellent signal';"
    "  else if(r>=-67) signal.textContent='Good signal';"
    "  else if(r>=-75) signal.textContent='Fair signal';"
    "  else signal.textContent='Weak signal';"
    "}"

    "ssid.addEventListener('change',function(){"
    "  updateSignal();"
    "  clearMsg();"
    "});"

    "eye.addEventListener('click',function(){"
    "  if(pass.type==='password'){"
    "    pass.type='text';"
    "    eye.setAttribute('aria-label','Hide password');"
    "  }else{"
    "    pass.type='password';"
    "    eye.setAttribute('aria-label','Show password');"
    "  }"
    "});"

    "async function scanWifi(){"

    "  ssid.innerHTML='<option value=\"\">Scanning nearby networks...</option>';"
    "  signal.textContent='';"

    "  try{"

    "    const r=await fetch('/scan',{cache:'no-store'});"
    "    const j=await r.json();"

    "    ssid.innerHTML='';"

    "    const first=document.createElement('option');"
    "    first.value='';"
    "    first.textContent='Select a Wi-Fi network';"
    "    ssid.appendChild(first);"

    "    if(!j.aps || j.aps.length===0){"
    "      showMsg('No Wi-Fi networks were found. Tap reload and try again.','error');"
    "      return;"
    "    }"

    "    for(const ap of j.aps){"
    "      if(!ap.ssid) continue;"

    "      const opt=document.createElement('option');"
    "      opt.value=ap.ssid;"
    "      opt.textContent=ap.ssid+'  ('+ap.rssi+' dBm)';"
    "      opt.dataset.rssi=ap.rssi;"
    "      opt.dataset.auth=ap.auth;"
    "      ssid.appendChild(opt);"
    "    }"

    "    updateSignal();"

    "  }catch(e){"
    "    ssid.innerHTML='<option value=\"\">Scan failed</option>';"
    "    showMsg('Unable to scan Wi-Fi networks.','error');"
    "  }"
    "}"

    "async function pollStatus(){"

    "  try{"

    "    const r=await fetch('/status',{"
    "      cache:'no-store',"
    "      headers:{'Cache-Control':'no-cache'}"
    "    });"

    "    const j=await r.json();"

    "    if(j.stage==='verifying'){"
    "      setBusy(true,'Connecting...');"
    "      showMsg('Connecting to '+ssid.value+'...','info');"
    "      return;"
    "    }"

    "    if(j.stage==='fail'){"

    "      if(statusTimer){"
    "        clearInterval(statusTimer);"
    "        statusTimer=null;"
    "      }"

    "      setBusy(false,'Connect');"

    "      showMsg(j.msg||'Wi-Fi connection failed.','error');"

    "      pass.value='';"
    "      pass.focus();"

    "      return;"
    "    }"

    "    if(j.stage==='saved'){"

    "      if(statusTimer){"
    "        clearInterval(statusTimer);"
    "        statusTimer=null;"
    "      }"

    "      setBusy(true,'Setup successful');"

    "      showMsg("
    "        'Wi-Fi configured successfully. ESP32 is restarting...',"
    "        'success'"
    "      );"

    "      return;"
    "    }"

    "  }catch(e){"
    "    /*"
    "     * ESP32 重启时 HTTP 连接会突然断开。"
    "     * 这里不能立刻显示失败。"
    "     */"
    "  }"
    "}"

    "form.addEventListener('submit',async function(e){"

    "  e.preventDefault();"

    "  clearMsg();"

    "  if(!ssid.value){"
    "    showMsg('Please select a Wi-Fi network.','error');"
    "    return;"
    "  }"

    "  const opt=ssid.options[ssid.selectedIndex];"
    "  const auth=Number(opt.dataset.auth||0);"

    "  if(auth!==0 && !pass.value){"
    "    showMsg('Please enter the Wi-Fi password.','error');"
    "    pass.focus();"
    "    return;"
    "  }"

    "  setBusy(true,'Connecting...');"
    "  showMsg('Checking Wi-Fi credentials...','info');"

    "  try{"

    "    const body="
    "      'ssid='+encodeURIComponent(ssid.value)+"
    "      '&pass='+encodeURIComponent(pass.value);"

    "    const r=await fetch('/save',{"
    "      method:'POST',"
    "      headers:{"
    "        'Content-Type':'application/x-www-form-urlencoded'"
    "      },"
    "      body:body,"
    "      cache:'no-store'"
    "    });"

    "    const j=await r.json();"

    "    if(j.stage==='verifying'){"

    "      if(statusTimer)"
    "        clearInterval(statusTimer);"

    "      statusTimer=setInterval(pollStatus,500);"

    "      pollStatus();"

    "      return;"
    "    }"

    "    if(j.stage==='fail'){"

    "      setBusy(false,'Connect');"

    "      showMsg(j.msg||'Wi-Fi connection failed.','error');"

    "      return;"
    "    }"

    "  }catch(e){"

    "    setBusy(false,'Connect');"

    "    showMsg('Unable to communicate with the ESP32.','error');"
    "  }"
    "});"

    "scanWifi();"

    "</script>"

    "</body>"
    "</html>";

// ============================================================
// 工具函数
// ============================================================

static void url_decode(char *s)
{
    char *d = s;

    while (*s)
    {
        if (*s == '+')
        {
            *d++ = ' ';
            s++;
        }
        else if (*s == '%' && s[1] && s[2])
        {
            char hex[3] = {
                s[1],
                s[2],
                0};

            *d++ = (char)strtoul(hex, NULL, 16);
            s += 3;
        }
        else
        {
            *d++ = *s++;
        }
    }

    *d = '\0';
}

static void form_get(
    const char *body,
    const char *key,
    char *out,
    size_t outsz)
{
    if (!out || outsz == 0)
        return;

    out[0] = '\0';

    size_t key_len = strlen(key);
    const char *p = body;

    while (*p)
    {

        const char *e = strchr(p, '&');

        size_t len = e
                         ? (size_t)(e - p)
                         : strlen(p);

        if (len >= key_len &&
            strncmp(p, key, key_len) == 0)
        {

            size_t value_len = len - key_len;

            if (value_len >= outsz)
                value_len = outsz - 1;

            memcpy(
                out,
                p + key_len,
                value_len);

            out[value_len] = '\0';

            url_decode(out);
            return;
        }

        if (!e)
            break;

        p = e + 1;
    }
}

static void json_escape(
    const char *src,
    char *dst,
    size_t dstsz)
{
    if (!dst || dstsz == 0)
        return;

    size_t j = 0;

    for (size_t i = 0; src[i] && j + 2 < dstsz; i++)
    {

        unsigned char c = (unsigned char)src[i];

        if (c == '"' || c == '\\')
        {
            if (j + 2 >= dstsz)
                break;

            dst[j++] = '\\';
            dst[j++] = (char)c;
        }
        else if (c == '\n')
        {
            dst[j++] = '\\';
            dst[j++] = 'n';
        }
        else if (c == '\r')
        {
            dst[j++] = '\\';
            dst[j++] = 'r';
        }
        else if (c == '\t')
        {
            dst[j++] = '\\';
            dst[j++] = 't';
        }
        else if (c < 0x20)
        {
            continue;
        }
        else
        {
            dst[j++] = (char)c;
        }
    }

    dst[j] = '\0';
}

// ============================================================
// 状态机
// ============================================================

enum
{
    SAVE_IDLE = 0,
    SAVE_VERIFYING,
    SAVE_SUCCESS,
    SAVE_FAIL
};

static volatile int s_save_stage = SAVE_IDLE;
static volatile int s_save_inflight = 0;

static char s_save_msg[160] = {0};

static portMUX_TYPE s_save_mux =
    portMUX_INITIALIZER_UNLOCKED;

// ============================================================
// Wi-Fi 验证
// ============================================================

typedef enum
{
    VERIFY_NONE = 0,
    VERIFY_OK,
    VERIFY_AUTH_FAIL,
    VERIFY_NO_AP,
    VERIFY_ASSOC_FAIL,
    VERIFY_TIMEOUT,
    VERIFY_OTHER
} verify_result_t;

typedef struct
{
    bool got_ip;
    verify_result_t result;
} verify_state_t;

static void verify_event_handler(
    void *arg,
    esp_event_base_t base,
    int32_t id,
    void *data)
{
    verify_state_t *st =
        (verify_state_t *)arg;

    if (!st)
        return;

    if (base == IP_EVENT &&
        id == IP_EVENT_STA_GOT_IP)
    {

        st->got_ip = true;
        st->result = VERIFY_OK;

        ESP_LOGI(
            TAG,
            "verify: GOT_IP");

        return;
    }

    if (base == WIFI_EVENT &&
        id == WIFI_EVENT_STA_DISCONNECTED)
    {

        wifi_event_sta_disconnected_t *e =
            (wifi_event_sta_disconnected_t *)data;

        int reason = e
                         ? e->reason
                         : -1;

        ESP_LOGW(
            TAG,
            "verify: disconnected reason=%d",
            reason);

        if (!e)
            return;

        switch (e->reason)
        {

        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
            st->result = VERIFY_AUTH_FAIL;
            break;

#ifdef WIFI_REASON_NO_AP_FOUND
        case WIFI_REASON_NO_AP_FOUND:
            st->result = VERIFY_NO_AP;
            break;
#endif

#ifdef WIFI_REASON_ASSOC_FAIL
        case WIFI_REASON_ASSOC_FAIL:
            st->result = VERIFY_ASSOC_FAIL;
            break;
#endif

        default:
            if (st->result == VERIFY_NONE)
                st->result = VERIFY_OTHER;
            break;
        }
    }
}

static const char *verify_error_text(
    verify_result_t result)
{
    switch (result)
    {

    case VERIFY_AUTH_FAIL:
        return "Wi-Fi password is incorrect.";

    case VERIFY_NO_AP:
        return "Wi-Fi network was not found.";

    case VERIFY_ASSOC_FAIL:
        return "Could not connect to this Wi-Fi network.";

    case VERIFY_TIMEOUT:
        return "Connection timed out. Check signal strength and Wi-Fi settings.";

    default:
        return "Could not connect to this Wi-Fi network.";
    }
}

static bool try_sta_connect(
    const char *ssid,
    const char *pass,
    int timeout_ms,
    verify_result_t *out_result)
{
    if (!ssid || !out_result)
        return false;

    *out_result = VERIFY_NONE;

    verify_state_t st = {
        .got_ip = false,
        .result = VERIFY_NONE
    };

    esp_event_handler_instance_t h_disconnect = NULL;
    esp_event_handler_instance_t h_got_ip = NULL;

    esp_err_t err = ESP_OK;

    // 已经等待的时间，单位 ms
    int waited = 0;

    // ========================================================
    // 注册 Wi-Fi 断开事件
    // ========================================================

    err = esp_event_handler_instance_register(
        WIFI_EVENT,
        WIFI_EVENT_STA_DISCONNECTED,
        &verify_event_handler,
        &st,
        &h_disconnect
    );

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "register disconnect handler failed: %s",
            esp_err_to_name(err)
        );

        *out_result = VERIFY_OTHER;

        return false;
    }

    // ========================================================
    // 注册 GOT_IP 事件
    // ========================================================

    err = esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &verify_event_handler,
        &st,
        &h_got_ip
    );

    if (err != ESP_OK)
    {
        esp_event_handler_instance_unregister(
            WIFI_EVENT,
            WIFI_EVENT_STA_DISCONNECTED,
            h_disconnect
        );

        ESP_LOGE(
            TAG,
            "register got_ip handler failed: %s",
            esp_err_to_name(err)
        );

        *out_result = VERIFY_OTHER;

        return false;
    }

    // ========================================================
    // 保持原来的 APSTA 配网结构
    //
    // 这里不要切换 WIFI_MODE_STA。
    // 手机仍然连接着 PCMonitor-Setup，
    // ESP32 同时使用 STA 去连接目标 Wi-Fi。
    // ========================================================

    // 先断开可能存在的旧 STA 连接
    esp_wifi_disconnect();

    vTaskDelay(
        pdMS_TO_TICKS(200)
    );

    // ========================================================
    // 设置目标 Wi-Fi
    // ========================================================

    wifi_config_t wc = {};

    strncpy(
        (char *)wc.sta.ssid,
        ssid,
        sizeof(wc.sta.ssid) - 1
    );

    if (pass)
    {
        strncpy(
            (char *)wc.sta.password,
            pass,
            sizeof(wc.sta.password) - 1
        );
    }

    // 不强制 WPA2
    wc.sta.pmf_cfg.capable = true;
    wc.sta.pmf_cfg.required = false;

    err = esp_wifi_set_config(
        WIFI_IF_STA,
        &wc
    );

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "esp_wifi_set_config failed: %s",
            esp_err_to_name(err)
        );

        *out_result = VERIFY_OTHER;

        goto cleanup_fail;
    }

    // ========================================================
    // 第一次连接
    // ========================================================

    ESP_LOGI(
        TAG,
        "verify: connecting to '%s'",
        ssid
    );

    err = esp_wifi_connect();

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "esp_wifi_connect failed: %s",
            esp_err_to_name(err)
        );

        *out_result = VERIFY_OTHER;

        goto cleanup_fail;
    }

    // ========================================================
    // 等待连接
    //
    // 最长 30 秒。
    //
    // 5 秒后如果还没有 GOT_IP，
    // 自动再调用一次 esp_wifi_connect()。
    //
    // 这样可以处理第一次连接比较慢的情况，
    // 同时不会关闭配网 AP。
    // ========================================================

    while (waited < timeout_ms)
    {
        // ----------------------------------------------------
        // 成功拿到 IP
        // ----------------------------------------------------

        if (st.got_ip)
        {
            ESP_LOGI(
                TAG,
                "verify: Wi-Fi connected successfully"
            );

            *out_result = VERIFY_OK;

            goto cleanup_success;
        }

        // ----------------------------------------------------
        // 明确的认证失败
        // ----------------------------------------------------

        if (st.result == VERIFY_AUTH_FAIL)
        {
            *out_result = VERIFY_AUTH_FAIL;

            ESP_LOGW(
                TAG,
                "verify: authentication failed"
            );

            goto cleanup_fail;
        }

        // ----------------------------------------------------
        // 找不到 AP
        // ----------------------------------------------------

        if (st.result == VERIFY_NO_AP)
        {
            *out_result = VERIFY_NO_AP;

            ESP_LOGW(
                TAG,
                "verify: target AP not found"
            );

            goto cleanup_fail;
        }

        // ----------------------------------------------------
        // 关联失败
        // ----------------------------------------------------

        if (st.result == VERIFY_ASSOC_FAIL)
        {
            *out_result = VERIFY_ASSOC_FAIL;

            ESP_LOGW(
                TAG,
                "verify: association failed"
            );

            goto cleanup_fail;
        }

        // ----------------------------------------------------
        // 其他断开
        //
        // 任何非认证/找不到/关联失败的断开都直接退出本次尝试，
        // 让 verify_task() 在新的 try_sta_connect() 里再发。
        // ----------------------------------------------------

        if (st.result == VERIFY_OTHER)
        {
            *out_result = VERIFY_OTHER;

            ESP_LOGW(
                TAG,
                "verify: connection attempt failed, "
                "retry from a fresh session"
            );

            goto cleanup_fail;
        }

        // ----------------------------------------------------
        // 每 200ms 检查一次
        // ----------------------------------------------------

        vTaskDelay(
            pdMS_TO_TICKS(200)
        );

        waited += 200;
    }

    // ========================================================
    // 超时
    // ========================================================

    *out_result = VERIFY_TIMEOUT;

    ESP_LOGW(
        TAG,
        "verify: timeout after %d ms",
        waited
    );

    esp_wifi_disconnect();

cleanup_fail:

    // ========================================================
    // 注销事件
    // ========================================================

    if (h_disconnect)
    {
        esp_event_handler_instance_unregister(
            WIFI_EVENT,
            WIFI_EVENT_STA_DISCONNECTED,
            h_disconnect
        );

        h_disconnect = NULL;
    }

    if (h_got_ip)
    {
        esp_event_handler_instance_unregister(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            h_got_ip
        );

        h_got_ip = NULL;
    }

    return false;

cleanup_success:

    // ========================================================
    // 成功：只注销事件。
    //
    // 不在这里关闭 AP。
    // 后面的 verify_task() 会保存 NVS，
    // 然后按原来的流程 esp_restart()。
    // ========================================================

    if (h_disconnect)
    {
        esp_event_handler_instance_unregister(
            WIFI_EVENT,
            WIFI_EVENT_STA_DISCONNECTED,
            h_disconnect
        );

        h_disconnect = NULL;
    }

    if (h_got_ip)
    {
        esp_event_handler_instance_unregister(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            h_got_ip
        );

        h_got_ip = NULL;
    }

    return true;
}

// ============================================================
// 后台验证任务
// ============================================================

static void verify_task(void *arg)
{
    char *payload = (char *)arg;

    if (!payload)
    {
        vTaskDelete(NULL);
        return;
    }

    char *ssid = payload;

    char *pass =
        ssid + strlen(ssid) + 1;

    verify_result_t result =
        VERIFY_NONE;

    // ========================================================
    // 一次提交级别的多次尝试
    //
    // 每次 try_sta_connect() 都是全新注册事件、
    // 全新 disconnect+connect，不会被上一次的
    // 中间状态拖死。
    // ========================================================

    bool ok = false;

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        result = VERIFY_NONE;

        ESP_LOGI(
            TAG,
            "Wi-Fi verification attempt %d/3",
            attempt
        );

        ok = try_sta_connect(
            ssid,
            pass,
            VERIFY_TIMEOUT_MS,
            &result
        );

        if (ok)
            break;

        ESP_LOGW(
            TAG,
            "verification attempt %d failed, "
            "preparing a fresh retry",
            attempt
        );

        esp_wifi_disconnect();

        vTaskDelay(
            pdMS_TO_TICKS(500)
        );
    }

    if (!ok)
    {

        ESP_LOGW(
            TAG,
            "verify failed: ssid='%s', result=%d",
            ssid,
            result
        );

        // ------------------------------------------------
        // 根据失败原因生成 UI 提示
        // ------------------------------------------------

        const char *ui_error =
            "Connection failed";

        switch (result)
        {
            case VERIFY_AUTH_FAIL:
                ui_error =
                    "Wrong password";
                break;

            case VERIFY_NO_AP:
                ui_error =
                    "Wi-Fi not found";
                break;

            case VERIFY_ASSOC_FAIL:
                ui_error =
                    "Wi-Fi connection failed";
                break;

            case VERIFY_TIMEOUT:
                ui_error =
                    "Connection timeout";
                break;

            default:
                ui_error =
                    "Connection failed";
                break;
        }

        NetSync_SetProvisionError(
            ui_error
        );

        NetSync_SetState(
            NETSYNC_STATE_PROVISION_ERROR
        );

        portENTER_CRITICAL(
            &s_save_mux
        );

        s_save_stage =
            SAVE_FAIL;

        snprintf(
            s_save_msg,
            sizeof(s_save_msg),
            "%s",
            verify_error_text(result)
        );

        s_save_inflight = 0;

        portEXIT_CRITICAL(
            &s_save_mux
        );

        free(payload);

        vTaskDelete(NULL);

        return;
    }

    // ========================================================
    // Wi-Fi 验证成功，现在才写 NVS
    // ========================================================

    nvs_handle_t h;

    esp_err_t err =
        nvs_open(
            "wifi",
            NVS_READWRITE,
            &h);

    if (err != ESP_OK)
    {

        ESP_LOGE(
            TAG,
            "nvs_open failed: %s",
            esp_err_to_name(err));

        esp_wifi_disconnect();

        portENTER_CRITICAL(
            &s_save_mux);

        s_save_stage = SAVE_FAIL;

        snprintf(
            s_save_msg,
            sizeof(s_save_msg),
            "Internal error: cannot open storage.");

        s_save_inflight = 0;

        portEXIT_CRITICAL(
            &s_save_mux);

        free(payload);

        vTaskDelete(NULL);
        return;
    }

    err = nvs_set_str(
        h,
        "ssid",
        ssid);

    if (err != ESP_OK)
    {

        ESP_LOGE(
            TAG,
            "nvs_set_str(ssid) failed: %s",
            esp_err_to_name(err));

        nvs_close(h);
        esp_wifi_disconnect();

        portENTER_CRITICAL(
            &s_save_mux);

        s_save_stage = SAVE_FAIL;

        snprintf(
            s_save_msg,
            sizeof(s_save_msg),
            "Internal error: cannot save Wi-Fi name.");

        s_save_inflight = 0;

        portEXIT_CRITICAL(
            &s_save_mux);

        free(payload);

        vTaskDelete(NULL);
        return;
    }

    err = nvs_set_str(
        h,
        "pass",
        pass);

    if (err != ESP_OK)
    {

        ESP_LOGE(
            TAG,
            "nvs_set_str(pass) failed: %s",
            esp_err_to_name(err));

        nvs_close(h);
        esp_wifi_disconnect();

        portENTER_CRITICAL(
            &s_save_mux);

        s_save_stage = SAVE_FAIL;

        snprintf(
            s_save_msg,
            sizeof(s_save_msg),
            "Internal error: cannot save Wi-Fi password.");

        s_save_inflight = 0;

        portEXIT_CRITICAL(
            &s_save_mux);

        free(payload);

        vTaskDelete(NULL);
        return;
    }

    err = nvs_commit(h);

    nvs_close(h);

    if (err != ESP_OK)
    {

        ESP_LOGE(
            TAG,
            "nvs_commit failed: %s",
            esp_err_to_name(err));

        esp_wifi_disconnect();

        portENTER_CRITICAL(
            &s_save_mux);

        s_save_stage = SAVE_FAIL;

        snprintf(
            s_save_msg,
            sizeof(s_save_msg),
            "Internal error: cannot commit Wi-Fi settings.");

        s_save_inflight = 0;

        portEXIT_CRITICAL(
            &s_save_mux);

        free(payload);

        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(
        TAG,
        "Wi-Fi credentials saved successfully");

    portENTER_CRITICAL(
        &s_save_mux);

    s_save_stage = SAVE_SUCCESS;

    snprintf(
        s_save_msg,
        sizeof(s_save_msg),
        "Wi-Fi configured successfully.");

    s_save_inflight = 0;

    portEXIT_CRITICAL(
        &s_save_mux);

    // 通知 main 循环切到成功页 (PROVISION_SUCCESS)
    // 网络 task 不直接操作 LVGL。
    NetSync_SetState(
        NETSYNC_STATE_PROVISION_SUCCESS
    );

    // --------------------------------------------------------
    // 不再 esp_restart()
    // --------------------------------------------------------
    //
    // WifiProvision_StartAP() 会看到 SAVE_SUCCESS，
    // 然后负责：
    //
    //   1. 停止 HTTP
    //   2. 关闭配网 AP
    //   3. 切换到正常 STA
    //   4. 返回 NetSync
    //
    // 这样屏幕不会黑屏。
    // --------------------------------------------------------

    free(payload);

    ESP_LOGI(
        TAG,
        "provisioning finished, handing control back to NetSync");

    vTaskDelete(NULL);
}

// ============================================================
// HTTP 首页
// ============================================================

static esp_err_t h_index(
    httpd_req_t *req)
{
    ESP_LOGI(
        TAG,
        "HTTP GET /");

    httpd_resp_set_type(
        req,
        "text/html; charset=utf-8");

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-store, no-cache, must-revalidate, max-age=0");

    httpd_resp_set_hdr(
        req,
        "Pragma",
        "no-cache");

    return httpd_resp_send(
        req,
        INDEX_HTML,
        HTTPD_RESP_USE_STRLEN);
}

// ============================================================
// Wi-Fi 扫描 JSON
// ============================================================
static esp_err_t h_scan(
    httpd_req_t *req)
{
    ESP_LOGI(
        TAG,
        "HTTP GET /scan (cached)");

    char *out =
        (char *)malloc(8192);

    if (!out)
    {

        httpd_resp_set_status(
            req,
            "500 Internal Server Error");

        return httpd_resp_send(
            req,
            "memory error",
            HTTPD_RESP_USE_STRLEN);
    }

    size_t used =
        snprintf(
            out,
            8192,
            "{\"aps\":[");

    bool first = true;

    for (uint16_t i = 0;
         i < s_scan_count;
         i++)
    {

        if (s_scan_records[i].ssid[0] == '\0')
            continue;

        char escaped[128];

        json_escape(
            (const char *)s_scan_records[i].ssid,
            escaped,
            sizeof(escaped));

        if (!first)
        {

            used += snprintf(
                out + used,
                8192 - used,
                ",");
        }

        first = false;

        used += snprintf(
            out + used,
            8192 - used,
            "{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}",
            escaped,
            s_scan_records[i].rssi,
            (int)s_scan_records[i].authmode);

        if (used >= 7800)
            break;
    }

    snprintf(
        out + used,
        8192 - used,
        "]}");

    httpd_resp_set_type(
        req,
        "application/json");

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-store");

    esp_err_t ret =
        httpd_resp_send(
            req,
            out,
            HTTPD_RESP_USE_STRLEN);

    free(out);

    return ret;
}
// ============================================================
// HTTP /save
// ============================================================

static esp_err_t h_save(
    httpd_req_t *req)
{
    ESP_LOGI(
        TAG,
        "HTTP POST /save");

    if (req->content_len <= 0 ||
        req->content_len >= 512)
    {

        httpd_resp_set_status(
            req,
            "400 Bad Request");

        return httpd_resp_send(
            req,
            "{\"stage\":\"fail\",\"msg\":\"Request too large.\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    char buf[512];

    int total = 0;

    while (total < req->content_len)
    {

        int n =
            httpd_req_recv(
                req,
                buf + total,
                req->content_len - total);

        if (n <= 0)
        {

            ESP_LOGW(
                TAG,
                "httpd_req_recv failed");

            return ESP_FAIL;
        }

        total += n;
    }

    buf[total] = '\0';

    char ssid[33] = {0};
    char pass[65] = {0};

    form_get(
        buf,
        "ssid=",
        ssid,
        sizeof(ssid));

    form_get(
        buf,
        "pass=",
        pass,
        sizeof(pass));

    if (ssid[0] == '\0')
    {

        httpd_resp_set_type(
            req,
            "application/json");

        return httpd_resp_send(
            req,
            "{\"stage\":\"fail\",\"msg\":\"Please select a Wi-Fi network.\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    if (strlen(ssid) > 32)
    {

        httpd_resp_set_type(
            req,
            "application/json");

        return httpd_resp_send(
            req,
            "{\"stage\":\"fail\",\"msg\":\"Wi-Fi name is too long.\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    ESP_LOGI(
        TAG,
        "/save: ssid='%s', password_length=%d",
        ssid,
        (int)strlen(pass));
    ProvisionUI_SetWifiName(ssid);

    ProvisionUI_ShowConnecting();

    portENTER_CRITICAL(
        &s_save_mux);

    if (s_save_inflight)
    {

        portEXIT_CRITICAL(
            &s_save_mux);

        httpd_resp_set_type(
            req,
            "application/json");

        return httpd_resp_send(
            req,
            "{\"stage\":\"verifying\",\"msg\":\"Already connecting...\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    s_save_inflight = 1;
    s_save_stage = SAVE_VERIFYING;
    s_save_msg[0] = '\0';

    // --------------------------------------------------------
    // 更新当前尝试连接的 Wi-Fi
    // --------------------------------------------------------

    NetSync_SetProvisionSSID(
        ssid
    );

    NetSync_SetProvisionError(
        ""
    );

    // --------------------------------------------------------
    // 通知 LVGL：开始连接
    // --------------------------------------------------------

    NetSync_SetState(
        NETSYNC_STATE_CONNECTING
    );

    portEXIT_CRITICAL(
        &s_save_mux);

    size_t payload_size =
        strlen(ssid) + 1 + strlen(pass) + 1;

    char *payload =
        (char *)malloc(payload_size);

    if (!payload)
    {

        portENTER_CRITICAL(
            &s_save_mux);

        s_save_stage = SAVE_FAIL;

        snprintf(
            s_save_msg,
            sizeof(s_save_msg),
            "Internal error: not enough memory.");

        s_save_inflight = 0;

        portEXIT_CRITICAL(
            &s_save_mux);

        httpd_resp_set_type(
            req,
            "application/json");

        return httpd_resp_send(
            req,
            "{\"stage\":\"fail\",\"msg\":\"Not enough memory.\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    strcpy(
        payload,
        ssid);

    strcpy(
        payload + strlen(ssid) + 1,
        pass);

    if (xTaskCreate(
            verify_task,
            "wifi_verify",
            6144,
            payload,
            5,
            NULL) != pdPASS)
    {

        free(payload);

        portENTER_CRITICAL(
            &s_save_mux);

        s_save_stage = SAVE_FAIL;

        snprintf(
            s_save_msg,
            sizeof(s_save_msg),
            "Internal error: could not start Wi-Fi verification.");

        s_save_inflight = 0;

        portEXIT_CRITICAL(
            &s_save_mux);

        httpd_resp_set_type(
            req,
            "application/json");

        return httpd_resp_send(
            req,
            "{\"stage\":\"fail\",\"msg\":\"Could not start verification.\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    httpd_resp_set_type(
        req,
        "application/json");

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-store");

    return httpd_resp_send(
        req,
        "{\"stage\":\"verifying\",\"msg\":\"Checking Wi-Fi credentials...\"}",
        HTTPD_RESP_USE_STRLEN);
}

// ============================================================
// HTTP /status
// ============================================================

static esp_err_t h_status(
    httpd_req_t *req)
{
    int stage;
    char msg[160];

    portENTER_CRITICAL(
        &s_save_mux);

    stage = s_save_stage;

    strncpy(
        msg,
        s_save_msg,
        sizeof(msg) - 1);

    msg[sizeof(msg) - 1] = '\0';

    portEXIT_CRITICAL(
        &s_save_mux);

    const char *name = "idle";

    if (stage == SAVE_VERIFYING)
        name = "verifying";
    else if (stage == SAVE_SUCCESS)
        name = "saved";
    else if (stage == SAVE_FAIL)
        name = "fail";

    char escaped[320];

    json_escape(
        msg,
        escaped,
        sizeof(escaped));

    char body[512];

    snprintf(
        body,
        sizeof(body),
        "{\"stage\":\"%s\",\"msg\":\"%s\"}",
        name,
        escaped);

    httpd_resp_set_type(
        req,
        "application/json");

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-store, no-cache, must-revalidate, max-age=0");

    return httpd_resp_send(
        req,
        body,
        HTTPD_RESP_USE_STRLEN);
}

// ============================================================
// Captive Portal
// ============================================================

static esp_err_t captive_html(
    httpd_req_t *req,
    const char *path)
{
    ESP_LOGI(
        TAG,
        "Captive Portal request: %s",
        path);

    httpd_resp_set_status(
        req,
        "200 OK");

    httpd_resp_set_hdr(
        req,
        "Cache-Control",
        "no-store, no-cache, must-revalidate, max-age=0");

    httpd_resp_set_hdr(
        req,
        "Pragma",
        "no-cache");

    httpd_resp_set_type(
        req,
        "text/html; charset=utf-8");

    const char *body =
        "<!doctype html>"
        "<html><head>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<meta http-equiv='refresh' "
        "content='0;url=http://192.168.4.1/'>"
        "</head><body>"
        "<p>Opening Wi-Fi setup...</p>"
        "<p><a href='http://192.168.4.1/'>Open setup</a></p>"
        "<script>"
        "location.replace('http://192.168.4.1/');"
        "</script>"
        "</body></html>";

    return httpd_resp_send(
        req,
        body,
        HTTPD_RESP_USE_STRLEN);
}

static esp_err_t h_generate_204(
    httpd_req_t *req)
{
    return captive_html(
        req,
        "/generate_204");
}

static esp_err_t h_hotspot_detect(
    httpd_req_t *req)
{
    return captive_html(
        req,
        "/hotspot-detect.html");
}

static esp_err_t h_connect_test(
    httpd_req_t *req)
{
    return captive_html(
        req,
        "/connecttest.txt");
}

static esp_err_t h_success_txt(
    httpd_req_t *req)
{
    return captive_html(
        req,
        "/success.txt");
}

static esp_err_t h_ncsi(
    httpd_req_t *req)
{
    return captive_html(
        req,
        "/ncsi.txt");
}

static esp_err_t h_kindle(
    httpd_req_t *req)
{
    return captive_html(
        req,
        "/kindle-wifi/wifistub.html");
}

// ============================================================
// DNS Server
// ============================================================

static void dns_task(void *arg)
{
    (void)arg;

    int s =
        socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP);

    if (s < 0)
    {

        ESP_LOGE(
            TAG,
            "DNS socket failed: errno=%d",
            errno);

        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr;

    memset(
        &addr,
        0,
        sizeof(addr));

    addr.sin_family =
        AF_INET;

    addr.sin_port =
        htons(PROV_DNS_PORT);

    addr.sin_addr.s_addr =
        htonl(INADDR_ANY);

    if (bind(
            s,
            (struct sockaddr *)&addr,
            sizeof(addr)) < 0)
    {

        ESP_LOGE(
            TAG,
            "DNS bind port 53 failed: errno=%d",
            errno);

        close(s);

        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(
        TAG,
        "DNS server started on UDP 53");

    uint8_t buf[512];
    uint8_t rsp[512];

    while (1)
    {

        struct sockaddr_in from;
        socklen_t fl =
            sizeof(from);

        int n =
            recvfrom(
                s,
                buf,
                sizeof(buf),
                0,
                (struct sockaddr *)&from,
                &fl);

        if (n < 12)
            continue;

        memset(
            rsp,
            0,
            sizeof(rsp));

        // ----------------------------------------------------
        // Header
        // ----------------------------------------------------

        memcpy(
            rsp,
            buf,
            2);

        rsp[2] = 0x81;
        rsp[3] = 0x80;

        uint16_t qdcount =
            ((uint16_t)buf[4] << 8) | buf[5];

        if (qdcount != 1)
        {

            ESP_LOGD(
                TAG,
                "DNS query qdcount=%u unsupported",
                qdcount);

            continue;
        }

        rsp[4] = 0;
        rsp[5] = 1;

        // Answer 后面根据 QTYPE 决定
        rsp[6] = 0;
        rsp[7] = 0;

        rsp[8] = 0;
        rsp[9] = 0;

        rsp[10] = 0;
        rsp[11] = 0;

        // ----------------------------------------------------
        // Question
        // ----------------------------------------------------

        int qoff = 12;

        while (qoff < n)
        {

            uint8_t len =
                buf[qoff];

            if (len == 0)
            {

                qoff++;

                break;
            }

            // DNS 压缩指针
            if ((len & 0xC0) == 0xC0)
            {

                if (qoff + 1 >= n)
                    break;

                qoff += 2;

                break;
            }

            if (len > 63)
                break;

            if (qoff + 1 + len >= n)
                break;

            qoff +=
                1 + len;
        }

        // QNAME + zero + QTYPE(2) + QCLASS(2)
        if (qoff <= 12 ||
            qoff + 4 > n)
        {

            continue;
        }

        int qlen =
            (qoff - 12) + 4;

        if (12 + qlen > (int)sizeof(rsp))
            continue;

        memcpy(
            rsp + 12,
            buf + 12,
            qlen);

        // ----------------------------------------------------
        // 获取 QTYPE
        // ----------------------------------------------------

        int qtype_offset =
            12 + (qoff - 12);

        uint16_t qtype =
            ((uint16_t)buf[qtype_offset] << 8) | buf[qtype_offset + 1];

        int off =
            12 + qlen;

        // ----------------------------------------------------
        // A 查询 → 返回 192.168.4.1
        // AAAA 等 → 返回空答案
        // ----------------------------------------------------

        if (qtype == 1)
        {

            rsp[6] = 0;
            rsp[7] = 1;

            // NAME pointer
            rsp[off++] = 0xC0;
            rsp[off++] = 0x0C;

            // TYPE A
            rsp[off++] = 0;
            rsp[off++] = 1;

            // CLASS IN
            rsp[off++] = 0;
            rsp[off++] = 1;

            // TTL = 120
            rsp[off++] = 0;
            rsp[off++] = 0;
            rsp[off++] = 0;
            rsp[off++] = 120;

            // IPv4 长度
            rsp[off++] = 0;
            rsp[off++] = 4;

            uint32_t ip =
                inet_addr(PROV_AP_IP);

            memcpy(
                rsp + off,
                &ip,
                4);

            off += 4;
        }

        ESP_LOGD(
            TAG,
            "DNS query type=%u -> %s",
            qtype,
            qtype == 1
                ? PROV_AP_IP
                : "NODATA");

        sendto(
            s,
            rsp,
            off,
            0,
            (struct sockaddr *)&from,
            fl);
    }
}

// ============================================================
// NVS API
// ============================================================

int WifiProvision_Load(
    char *ssid,
    size_t ssid_len,
    char *pass,
    size_t pass_len)
{
    if (!ssid ||
        !pass ||
        ssid_len == 0 ||
        pass_len == 0)
    {

        return 0;
    }

    ssid[0] = '\0';
    pass[0] = '\0';

    nvs_handle_t h;

    if (nvs_open(
            "wifi",
            NVS_READONLY,
            &h) != ESP_OK)
    {

        return 0;
    }

    size_t s_len = ssid_len;
    size_t p_len = pass_len;

    esp_err_t e1 =
        nvs_get_str(
            h,
            "ssid",
            ssid,
            &s_len);

    esp_err_t e2 =
        nvs_get_str(
            h,
            "pass",
            pass,
            &p_len);

    nvs_close(h);

    if (e1 != ESP_OK ||
        e2 != ESP_OK)
    {

        ssid[0] = '\0';
        pass[0] = '\0';

        return 0;
    }

    if (ssid[0] == '\0')
        return 0;

    return 1;
}

void WifiProvision_ClearCredentials(void)
{
    nvs_handle_t h;

    if (nvs_open(
            "wifi",
            NVS_READWRITE,
            &h) != ESP_OK)
    {

        return;
    }

    nvs_erase_key(
        h,
        "ssid");

    nvs_erase_key(
        h,
        "pass");

    nvs_commit(h);

    nvs_close(h);

    ESP_LOGI(
        TAG,
        "saved Wi-Fi credentials cleared");
}

static void provision_scan_wifi(void)
{
    s_scan_count = 0;

    ESP_LOGI(
        TAG,
        "starting Wi-Fi scan before AP...");

    wifi_scan_config_t scan_cfg = {};

    esp_err_t err =
        esp_wifi_scan_start(
            &scan_cfg,
            true);

    if (err != ESP_OK)
    {

        ESP_LOGW(
            TAG,
            "initial Wi-Fi scan failed: %s",
            esp_err_to_name(err));

        return;
    }

    uint16_t n = 0;

    err =
        esp_wifi_scan_get_ap_num(&n);

    if (err != ESP_OK)
    {

        ESP_LOGW(
            TAG,
            "get AP count failed: %s",
            esp_err_to_name(err));

        return;
    }

    if (n > MAX_SCAN_AP)
        n = MAX_SCAN_AP;

    err =
        esp_wifi_scan_get_ap_records(
            &n,
            s_scan_records);

    if (err != ESP_OK)
    {

        ESP_LOGW(
            TAG,
            "get AP records failed: %s",
            esp_err_to_name(err));

        return;
    }

    s_scan_count = n;

    // RSSI 从强到弱排序
    for (int i = 0; i < (int)s_scan_count - 1; i++)
    {

        for (int j = i + 1;
             j < (int)s_scan_count;
             j++)
        {

            if (
                s_scan_records[j].rssi >
                s_scan_records[i].rssi)
            {

                wifi_ap_record_t tmp =
                    s_scan_records[i];

                s_scan_records[i] =
                    s_scan_records[j];

                s_scan_records[j] =
                    tmp;
            }
        }
    }

    ESP_LOGI(
        TAG,
        "Wi-Fi scan complete: %u APs",
        s_scan_count);
}

// ============================================================
// 启动 AP 配网
// ============================================================

bool WifiProvision_StartAP(void)
{
    ESP_LOGI(
        TAG,
        "starting Wi-Fi provisioning");

    // --------------------------------------------------------
    // 网络栈初始化
    // --------------------------------------------------------

    static bool netif_inited = false;

    if (!netif_inited)
    {

        ESP_ERROR_CHECK(
            esp_netif_init());

        ESP_ERROR_CHECK(
            esp_event_loop_create_default());

        netif_inited = true;
    }

    // --------------------------------------------------------
    // 创建 AP + STA netif
    // --------------------------------------------------------

    esp_netif_t *ap_netif =
        esp_netif_create_default_wifi_ap();

    esp_netif_t *sta_netif =
        esp_netif_create_default_wifi_sta();

    if (!ap_netif || !sta_netif)
    {

        ESP_LOGE(
            TAG,
            "failed to create Wi-Fi netif");

        return false;
    }

    // --------------------------------------------------------
    // AP DHCP
    // --------------------------------------------------------

    ESP_ERROR_CHECK(
        esp_netif_dhcps_stop(
            ap_netif));

    esp_netif_dns_info_t dns;

    memset(
        &dns,
        0,
        sizeof(dns));

    dns.ip.type =
        IPADDR_TYPE_V4;

    dns.ip.u_addr.ip4.addr =
        inet_addr(PROV_AP_IP);

    ESP_ERROR_CHECK(
        esp_netif_set_dns_info(
            ap_netif,
            ESP_NETIF_DNS_MAIN,
            &dns));

    // --------------------------------------------------------
    // DHCP Option 114 Captive Portal URI
    // --------------------------------------------------------

    static const char portal_uri[] =
        "http://192.168.4.1/";

    esp_err_t portal_err =
        esp_netif_dhcps_option(
            ap_netif,
            ESP_NETIF_OP_SET,
            ESP_NETIF_CAPTIVEPORTAL_URI,
            (void *)portal_uri,
            strlen(portal_uri));

    ESP_LOGI(
        TAG,
        "DHCP captive portal URI: %s, result=%s",
        portal_uri,
        esp_err_to_name(portal_err));

    ESP_ERROR_CHECK(
        esp_netif_dhcps_start(
            ap_netif));

    // --------------------------------------------------------
    // Wi-Fi driver
    // --------------------------------------------------------

    static bool wifi_inited = false;

    if (!wifi_inited)
    {

        wifi_init_config_t cfg =
            WIFI_INIT_CONFIG_DEFAULT();

        ESP_ERROR_CHECK(
            esp_wifi_init(&cfg));

        wifi_inited = true;
    }

    // --------------------------------------------------------
    // APSTA
    // --------------------------------------------------------

    wifi_config_t ap_cfg = {};

    strncpy(
        (char *)ap_cfg.ap.ssid,
        PROV_AP_SSID,
        sizeof(ap_cfg.ap.ssid) - 1);

    ap_cfg.ap.ssid_len =
        strlen(PROV_AP_SSID);

    ap_cfg.ap.channel = 1;

    ap_cfg.ap.authmode =
        WIFI_AUTH_OPEN;

    ap_cfg.ap.max_connection =
        4;

    ap_cfg.ap.pmf_cfg.required =
        false;

    // --------------------------------------------------------
    // 先启动 STA，只用于扫描附近 Wi-Fi
    // 此时 AP 尚未启动，所以扫描不会影响手机连接
    // --------------------------------------------------------

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(
            WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_start());

    ESP_ERROR_CHECK(
        esp_wifi_set_ps(
            WIFI_PS_NONE));

    // 给 Wi-Fi 一点时间初始化
    vTaskDelay(
        pdMS_TO_TICKS(200));

    // 扫描一次
    provision_scan_wifi();

    // 扫描结束后停止 Wi-Fi
    ESP_ERROR_CHECK(
        esp_wifi_stop());

    // --------------------------------------------------------
    // 再启动 APSTA
    // --------------------------------------------------------

    wifi_config_t empty_sta = {};

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(
            WIFI_MODE_APSTA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_AP,
            &ap_cfg));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &empty_sta));

    ESP_ERROR_CHECK(
        esp_wifi_start());

    ESP_ERROR_CHECK(
        esp_wifi_set_ps(
            WIFI_PS_NONE));

    vTaskDelay(
        pdMS_TO_TICKS(500));

    ESP_LOGI(
        TAG,
        "AP started: %s / %s",
        PROV_AP_SSID,
        PROV_AP_IP);

    // --------------------------------------------------------
    // HTTP server
    // --------------------------------------------------------

    httpd_config_t http_cfg =
        HTTPD_DEFAULT_CONFIG();

    http_cfg.server_port =
        PROV_HTTP_PORT;

    http_cfg.max_uri_handlers =
        16;

    http_cfg.lru_purge_enable =
        true;

    httpd_handle_t server = NULL;

    esp_err_t http_err =
        httpd_start(
            &server,
            &http_cfg);

    if (http_err != ESP_OK)
    {

        ESP_LOGE(
            TAG,
            "httpd_start failed: %s",
            esp_err_to_name(http_err));
    }
    else
    {

        httpd_uri_t u_index = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = h_index,
            .user_ctx = NULL};

        httpd_uri_t u_scan = {
            .uri = "/scan",
            .method = HTTP_GET,
            .handler = h_scan,
            .user_ctx = NULL};

        httpd_uri_t u_save = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = h_save,
            .user_ctx = NULL};

        httpd_uri_t u_status = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = h_status,
            .user_ctx = NULL};

        httpd_uri_t u_204 = {
            .uri = "/generate_204",
            .method = HTTP_GET,
            .handler = h_generate_204,
            .user_ctx = NULL};

        httpd_uri_t u_hotspot = {
            .uri = "/hotspot-detect.html",
            .method = HTTP_GET,
            .handler = h_hotspot_detect,
            .user_ctx = NULL};

        httpd_uri_t u_connect = {
            .uri = "/connecttest.txt",
            .method = HTTP_GET,
            .handler = h_connect_test,
            .user_ctx = NULL};

        httpd_uri_t u_success = {
            .uri = "/success.txt",
            .method = HTTP_GET,
            .handler = h_success_txt,
            .user_ctx = NULL};

        httpd_uri_t u_ncsi = {
            .uri = "/ncsi.txt",
            .method = HTTP_GET,
            .handler = h_ncsi,
            .user_ctx = NULL};

        httpd_uri_t u_kindle = {
            .uri = "/kindle-wifi/wifistub.html",
            .method = HTTP_GET,
            .handler = h_kindle,
            .user_ctx = NULL};

        httpd_register_uri_handler(
            server,
            &u_index);

        httpd_register_uri_handler(
            server,
            &u_scan);

        httpd_register_uri_handler(
            server,
            &u_save);

        httpd_register_uri_handler(
            server,
            &u_status);

        httpd_register_uri_handler(
            server,
            &u_204);

        httpd_register_uri_handler(
            server,
            &u_hotspot);

        httpd_register_uri_handler(
            server,
            &u_connect);

        httpd_register_uri_handler(
            server,
            &u_success);

        httpd_register_uri_handler(
            server,
            &u_ncsi);

        httpd_register_uri_handler(
            server,
            &u_kindle);

        ESP_LOGI(
            TAG,
            "HTTP server started");
    }

    // --------------------------------------------------------
    // DNS
    // --------------------------------------------------------

    BaseType_t dns_task_ret =
        xTaskCreate(
            dns_task,
            "dns_server",
            4096,
            NULL,
            3,
            NULL);

    if (dns_task_ret != pdPASS)
    {

        ESP_LOGE(
            TAG,
            "DNS task create failed");
    }

    // --------------------------------------------------------
    // 保持配网服务运行。
    // verify_task 成功以后会把 s_save_stage 设置为 SAVE_SUCCESS。
    // --------------------------------------------------------

    while (1)
    {
        int stage;

        portENTER_CRITICAL(
            &s_save_mux
        );

        stage = s_save_stage;

        portEXIT_CRITICAL(
            &s_save_mux
        );

        // ----------------------------------------------------
        // 配网成功
        // ----------------------------------------------------

        if (stage == SAVE_SUCCESS)
        {
            ESP_LOGI(
                TAG,
                "provisioning success detected"
            );

            break;
        }

        // ----------------------------------------------------
        // 继续等待手机提交 Wi-Fi
        // ----------------------------------------------------

        vTaskDelay(
            pdMS_TO_TICKS(200)
        );
    }

    // ========================================================
    // 配网成功：开始退出 AP 模式
    // ========================================================

    ESP_LOGI(
        TAG,
        "stopping provisioning HTTP server"
    );

    if (server)
    {
        httpd_stop(server);
        server = NULL;
    }

    // --------------------------------------------------------
    // 从 APSTA 切换到普通 STA
    //
    // 注意：这一步发生在 Wi-Fi 已经验证成功以后。
    // 和之前错误的 try_sta_connect() 修改不同。
    // --------------------------------------------------------

    ESP_LOGI(
        TAG,
        "switching APSTA -> STA"
    );

    esp_err_t mode_err =
        esp_wifi_set_mode(
            WIFI_MODE_STA
        );

    if (mode_err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "failed to switch to STA mode: %s",
            esp_err_to_name(mode_err)
        );

        return false;
    }

    ESP_LOGI(
        TAG,
        "provisioning AP stopped, normal STA mode ready"
    );

    return true;
}