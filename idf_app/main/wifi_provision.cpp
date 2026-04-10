#include "wifi_provision.h"
#include "wifi_manager.h"

#include "dns_server.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{

constexpr const char* kTag = "wifi_prov";
constexpr int kMaxApList   = 20;

static httpd_handle_t       s_httpd  = nullptr;
static dns_server_handle_t  s_dns    = nullptr;
static esp_netif_t*         s_ap_netif = nullptr;
static bool                 s_active = false;
static char                 s_ap_ssid[33] = {};

// Last connection attempt result for /status polling
static volatile bool s_connecting   = false;
static volatile bool s_connect_ok   = false;
static volatile bool s_connect_fail = false;

// --- HTML form (embedded in flash) ---

static const char kHtmlPage[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RS520 Knob WiFi Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#eee;
     display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#16213e;border-radius:16px;padding:32px;width:min(90vw,380px);
      box-shadow:0 8px 32px rgba(0,0,0,.4)}
h1{font-size:1.3em;margin-bottom:8px;color:#e94560}
p.sub{font-size:.85em;color:#999;margin-bottom:20px}
label{display:block;font-size:.9em;margin:12px 0 4px;color:#aaa}
select,input[type=password],input[type=text]{width:100%;padding:10px 12px;
  border:1px solid #333;border-radius:8px;background:#0f3460;color:#eee;font-size:1em}
button{width:100%;padding:12px;margin-top:20px;border:none;border-radius:8px;
  background:#e94560;color:#fff;font-size:1em;cursor:pointer;font-weight:600}
button:disabled{background:#555}
.status{margin-top:16px;text-align:center;font-size:.9em}
.ok{color:#4ecca3}.err{color:#e94560}.spin{color:#e9c46a}
</style>
</head>
<body>
<div class="card">
<h1>RS520 Knob</h1>
<p class="sub">WiFi Setup</p>
<form id="f">
<label>Network</label>
<select id="ssid"><option>Scanning...</option></select>
<label>Password</label>
<input type="password" id="pass" autocomplete="off">
<button type="submit" id="btn">Connect</button>
</form>
<div class="status" id="st"></div>
</div>
<script>
var st=document.getElementById('st'),btn=document.getElementById('btn');
function scan(){
  fetch('/scan').then(r=>r.json()).then(d=>{
    var s=document.getElementById('ssid');s.innerHTML='';
    d.forEach(a=>{var o=document.createElement('option');
      o.value=a.ssid;o.textContent=a.ssid+' ('+a.rssi+'dBm)';s.appendChild(o)});
    if(!d.length){s.innerHTML='<option>No networks found</option>';}
  }).catch(()=>{st.textContent='Scan failed';st.className='status err'});
}
scan();
document.getElementById('f').onsubmit=function(e){
  e.preventDefault();btn.disabled=true;
  st.textContent='Connecting...';st.className='status spin';
  var fd=new URLSearchParams();
  fd.append('ssid',document.getElementById('ssid').value);
  fd.append('pass',document.getElementById('pass').value);
  fetch('/connect',{method:'POST',body:fd,
    headers:{'Content-Type':'application/x-www-form-urlencoded'}})
  .then(r=>r.json()).then(()=>{poll()})
  .catch(()=>{st.textContent='Request failed';st.className='status err';btn.disabled=false});
};
function poll(){
  setTimeout(function check(){
    fetch('/status').then(r=>r.json()).then(d=>{
      if(d.state==='connected'){st.textContent='Connected! You can close this page.';st.className='status ok';}
      else if(d.state==='failed'){st.textContent='Connection failed. Try again.';st.className='status err';btn.disabled=false;}
      else{setTimeout(check,1500);}
    }).catch(()=>{st.textContent='Connected! (AP closed)';st.className='status ok'});
  },2000);
}
</script>
</body>
</html>
)rawliteral";

// --- URL-decode helper ---

static void url_decode(char* dst, const char* src, size_t dst_size)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di < dst_size - 1; si++)
    {
        if (src[si] == '%' && src[si + 1] && src[si + 2])
        {
            char hex[3] = {src[si + 1], src[si + 2], '\0'};
            dst[di++] = static_cast<char>(strtol(hex, nullptr, 16));
            si += 2;
        }
        else if (src[si] == '+')
        {
            dst[di++] = ' ';
        }
        else
        {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

// --- HTTP handlers ---

static esp_err_t handler_root(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, kHtmlPage, sizeof(kHtmlPage) - 1);
    return ESP_OK;
}

static esp_err_t handler_scan(httpd_req_t* req)
{
    wifi_scan_config_t scan_cfg = {};
    scan_cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_cfg.scan_time.active.min = 100;
    scan_cfg.scan_time.active.max = 300;

    // Need STA interface for scanning — AP+STA mode already set
    esp_wifi_scan_start(&scan_cfg, true);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    uint16_t fetched = std::min(static_cast<uint16_t>(kMaxApList), ap_count);
    wifi_ap_record_t* ap_list = nullptr;

    // Allocate on heap — scan results can be large
    if (fetched > 0)
    {
        ap_list = static_cast<wifi_ap_record_t*>(
            malloc(fetched * sizeof(wifi_ap_record_t)));
        if (ap_list)
        {
            esp_wifi_scan_get_ap_records(&fetched, ap_list);
        }
        else
        {
            fetched = 0;
        }
    }
    else
    {
        esp_wifi_clear_ap_list();
    }

    // Build JSON response
    httpd_resp_set_type(req, "application/json");

    char buf[128];
    httpd_resp_sendstr_chunk(req, "[");
    for (uint16_t i = 0; i < fetched; i++)
    {
        // Skip hidden SSIDs
        if (ap_list[i].ssid[0] == '\0') continue;

        // Escape SSID for JSON (simple: skip quotes/backslashes)
        char safe_ssid[33];
        size_t j = 0;
        for (size_t k = 0; ap_list[i].ssid[k] && j < sizeof(safe_ssid) - 1; k++)
        {
            char c = static_cast<char>(ap_list[i].ssid[k]);
            if (c != '"' && c != '\\')
            {
                safe_ssid[j++] = c;
            }
        }
        safe_ssid[j] = '\0';

        snprintf(buf, sizeof(buf),
                 "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}",
                 (i > 0) ? "," : "",
                 safe_ssid, ap_list[i].rssi, ap_list[i].authmode);
        httpd_resp_sendstr_chunk(req, buf);
    }
    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, nullptr);  // finish chunked

    free(ap_list);
    return ESP_OK;
}

static esp_err_t handler_connect(httpd_req_t* req)
{
    char body[256] = {};
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    body[len] = '\0';

    // Parse form: ssid=...&pass=...
    char ssid_raw[65] = {};
    char pass_raw[65] = {};
    char ssid[33] = {};
    char pass[65] = {};

    // Extract ssid= value
    const char* ssid_start = strstr(body, "ssid=");
    if (!ssid_start)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid");
        return ESP_FAIL;
    }
    ssid_start += 5;
    const char* ssid_end = strchr(ssid_start, '&');
    size_t ssid_len = ssid_end ? static_cast<size_t>(ssid_end - ssid_start)
                               : strlen(ssid_start);
    if (ssid_len >= sizeof(ssid_raw)) ssid_len = sizeof(ssid_raw) - 1;
    memcpy(ssid_raw, ssid_start, ssid_len);
    ssid_raw[ssid_len] = '\0';
    url_decode(ssid, ssid_raw, sizeof(ssid));

    // Extract pass= value
    const char* pass_start = strstr(body, "pass=");
    if (pass_start)
    {
        pass_start += 5;
        const char* pass_end = strchr(pass_start, '&');
        size_t pass_len = pass_end ? static_cast<size_t>(pass_end - pass_start)
                                   : strlen(pass_start);
        if (pass_len >= sizeof(pass_raw)) pass_len = sizeof(pass_raw) - 1;
        memcpy(pass_raw, pass_start, pass_len);
        pass_raw[pass_len] = '\0';
        url_decode(pass, pass_raw, sizeof(pass));
    }

    ESP_LOGI(kTag, "Provisioning: SSID='%s'", ssid);

    // Store credentials
    rs520::wifi_store_credentials(ssid, pass);
    s_connecting   = true;
    s_connect_ok   = false;
    s_connect_fail = false;

    // Configure STA with new creds and trigger connection (mode is already APSTA)
    wifi_config_t sta_cfg = {};
    std::memcpy(sta_cfg.sta.ssid, ssid,
                std::min(std::strlen(ssid), sizeof(sta_cfg.sta.ssid)));
    std::memcpy(sta_cfg.sta.password, pass,
                std::min(std::strlen(pass), sizeof(sta_cfg.sta.password)));
    sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;

    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_connect();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"connecting\"}");

    return ESP_OK;
}

static esp_err_t handler_status(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");

    if (s_connect_ok)
    {
        httpd_resp_sendstr(req, "{\"state\":\"connected\"}");
    }
    else if (s_connect_fail)
    {
        httpd_resp_sendstr(req, "{\"state\":\"failed\"}");
    }
    else
    {
        httpd_resp_sendstr(req, "{\"state\":\"connecting\"}");
    }
    return ESP_OK;
}

// Catch-all handler → redirect to captive portal root
static esp_err_t handler_redirect(httpd_req_t* req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_sendstr(req, "Redirecting...");
    return ESP_OK;
}

static void build_ap_ssid()
{
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) == ESP_OK)
    {
        snprintf(s_ap_ssid, sizeof(s_ap_ssid), "RS520-Knob-%02X%02X%02X",
                 mac[3], mac[4], mac[5]);
    }
    else
    {
        snprintf(s_ap_ssid, sizeof(s_ap_ssid), "RS520-Knob-Setup");
    }
}

// Task: waits, then tears down provisioning AP after STA connected
static void prov_teardown_task(void* /*arg*/)
{
    // Give phone time to poll /status and see "connected"
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(kTag, "Tearing down provisioning AP");
    rs520::provision_stop();
    vTaskDelete(nullptr);
}

// WiFi event handler for provisioning — monitors STA connection result
static void prov_wifi_event_handler(void* /*arg*/, esp_event_base_t event_base,
                                    int32_t event_id, void* /*event_data*/)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        s_connect_ok   = true;
        s_connecting   = false;
        ESP_LOGI(kTag, "Provisioning: STA connected, stopping AP in 5s...");
        // Delayed teardown so phone can poll /status success
        xTaskCreate(prov_teardown_task, "prov_stop", 3072, nullptr, 2, nullptr);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_connecting)
        {
            s_connect_fail = true;
            s_connecting   = false;
            ESP_LOGW(kTag, "Provisioning: STA connection failed");
        }
    }
}

}  // namespace

namespace rs520
{

esp_err_t provision_start()
{
    if (s_active) return ESP_OK;

    ESP_LOGI(kTag, "Starting WiFi provisioning");
    wifi_set_state(WifiState::kProvisioning);

    build_ap_ssid();
    ESP_LOGI(kTag, "AP SSID: %s", s_ap_ssid);

    // Create AP netif
    s_ap_netif = esp_netif_create_default_wifi_ap();

    // Configure AP+STA mode (STA needed for scan + eventual connection)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap_cfg = {};
    std::memcpy(ap_cfg.ap.ssid, s_ap_ssid, std::strlen(s_ap_ssid));
    ap_cfg.ap.ssid_len       = static_cast<uint8_t>(std::strlen(s_ap_ssid));
    ap_cfg.ap.channel        = 1;
    ap_cfg.ap.max_connection = 1;
    ap_cfg.ap.authmode       = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Register provisioning-specific event handlers
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               prov_wifi_event_handler, nullptr);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                               prov_wifi_event_handler, nullptr);

    // DNS server — redirect all queries to AP IP
    dns_server_config_t dns_cfg = {};
    dns_cfg.num_of_entries = 1;
    dns_cfg.item[0].name   = "*";
    dns_cfg.item[0].if_key = "WIFI_AP_DEF";
    s_dns = start_dns_server(&dns_cfg);

    // HTTP server
    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    http_cfg.max_uri_handlers = 8;
    http_cfg.lru_purge_enable = true;
    http_cfg.uri_match_fn     = httpd_uri_match_wildcard;

    ESP_ERROR_CHECK(httpd_start(&s_httpd, &http_cfg));

    // Register URI handlers (order matters — specific before wildcard)
    const httpd_uri_t uri_root = {
        .uri = "/", .method = HTTP_GET, .handler = handler_root, .user_ctx = nullptr
    };
    const httpd_uri_t uri_scan = {
        .uri = "/scan", .method = HTTP_GET, .handler = handler_scan, .user_ctx = nullptr
    };
    const httpd_uri_t uri_connect = {
        .uri = "/connect", .method = HTTP_POST, .handler = handler_connect, .user_ctx = nullptr
    };
    const httpd_uri_t uri_status = {
        .uri = "/status", .method = HTTP_GET, .handler = handler_status, .user_ctx = nullptr
    };
    const httpd_uri_t uri_catch_all = {
        .uri = "/*", .method = HTTP_GET, .handler = handler_redirect, .user_ctx = nullptr
    };

    httpd_register_uri_handler(s_httpd, &uri_root);
    httpd_register_uri_handler(s_httpd, &uri_scan);
    httpd_register_uri_handler(s_httpd, &uri_connect);
    httpd_register_uri_handler(s_httpd, &uri_status);
    httpd_register_uri_handler(s_httpd, &uri_catch_all);

    s_active = true;
    return ESP_OK;
}

esp_err_t provision_stop()
{
    if (!s_active) return ESP_OK;

    ESP_LOGI(kTag, "Stopping provisioning");

    // Unregister provisioning event handlers
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                 prov_wifi_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                 prov_wifi_event_handler);

    if (s_httpd)
    {
        httpd_stop(s_httpd);
        s_httpd = nullptr;
    }

    if (s_dns)
    {
        stop_dns_server(s_dns);
        s_dns = nullptr;
    }

    // Switch APSTA → STA only (removes AP without killing STA connection)
    esp_wifi_set_mode(WIFI_MODE_STA);

    if (s_ap_netif)
    {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = nullptr;
    }

    s_active = false;
    return ESP_OK;
}

bool provision_active()
{
    return s_active;
}

const char* provision_ssid()
{
    return s_ap_ssid;
}

}  // namespace rs520
