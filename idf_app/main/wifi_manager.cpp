#include "wifi_manager.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/inet.h"

#include <algorithm>
#include <cstring>

namespace
{

constexpr const char* kTag          = "wifi_mgr";
constexpr const char* kNvsNamespace = "wifi";
constexpr const char* kNvsKeySsid   = "ssid";
constexpr const char* kNvsKeyPass   = "pass";
constexpr const char* kNvsKeyBssid  = "bssid";
constexpr const char* kNvsKeyChan   = "channel";

constexpr int kFastConnectTimeoutMs = 3000;
constexpr int kMaxRetries           = 3;
constexpr int kRoamIntervalMs       = 60000;
constexpr int kRoamHysteresis       = 8;  // dBm
constexpr int kRoamTaskStack        = 4096;
constexpr int kRoamTaskPrio         = 3;

// Event bits
constexpr int kBitConnected    = BIT0;
constexpr int kBitFail         = BIT1;
constexpr int kBitDisconnected = BIT2;

static EventGroupHandle_t s_event_group = nullptr;
static esp_netif_t*        s_sta_netif  = nullptr;
static rs520::WifiState    s_state      = rs520::WifiState::kDisconnected;
static int                 s_retry_count = 0;
static bool                s_fast_connect = false;

static rs520::WifiStateCallback s_state_cb  = nullptr;
static void*                    s_state_ctx = nullptr;

// Stored credentials for current connection attempt
static char    s_ssid[33]  = {};
static char    s_pass[65]  = {};
static uint8_t s_bssid[6]  = {};
static uint8_t s_channel   = 0;
static bool    s_has_bssid = false;

// Hostname buffer
static char s_hostname[32] = {};

static void set_state(rs520::WifiState new_state)
{
    if (s_state == new_state) return;
    s_state = new_state;
    ESP_LOGI(kTag, "State -> %d", static_cast<int>(new_state));
    if (s_state_cb)
    {
        s_state_cb(new_state, s_state_ctx);
    }
}

static void build_hostname()
{
    if (s_hostname[0] != '\0') return;

    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK)
    {
        snprintf(s_hostname, sizeof(s_hostname), "rs520-knob");
        return;
    }
    snprintf(s_hostname, sizeof(s_hostname), "rs520-knob-%02x%02x%02x",
             mac[3], mac[4], mac[5]);
}

// --- NVS helpers ---

static esp_err_t nvs_load_credentials()
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(kNvsNamespace, NVS_READONLY, &h);
    if (ret != ESP_OK) return ret;

    size_t len = sizeof(s_ssid);
    ret = nvs_get_str(h, kNvsKeySsid, s_ssid, &len);
    if (ret != ESP_OK)
    {
        nvs_close(h);
        return ret;
    }

    len = sizeof(s_pass);
    ret = nvs_get_str(h, kNvsKeyPass, s_pass, &len);
    if (ret != ESP_OK)
    {
        nvs_close(h);
        return ret;
    }

    // BSSID + channel are optional (fast connect)
    len = sizeof(s_bssid);
    s_has_bssid = (nvs_get_blob(h, kNvsKeyBssid, s_bssid, &len) == ESP_OK);

    if (s_has_bssid)
    {
        nvs_get_u8(h, kNvsKeyChan, &s_channel);
    }

    nvs_close(h);
    return ESP_OK;
}

static esp_err_t nvs_save_bssid_channel(const uint8_t* bssid, uint8_t channel)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(kNvsNamespace, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_blob(h, kNvsKeyBssid, bssid, 6);
    nvs_set_u8(h, kNvsKeyChan, channel);
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

// --- WiFi event handlers ---

static void wifi_event_handler(void* /*arg*/, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(kTag, "STA started, connecting...");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            auto* ev = static_cast<wifi_event_sta_disconnected_t*>(event_data);
            ESP_LOGW(kTag, "Disconnected, reason=%d", ev->reason);

            if (s_retry_count < kMaxRetries)
            {
                s_retry_count++;
                set_state(rs520::WifiState::kConnecting);

                // If was fast connect, fall back to scan-based
                if (s_fast_connect && s_retry_count == 1)
                {
                    ESP_LOGI(kTag, "Fast connect failed, falling back to scan");
                    s_fast_connect = false;

                    wifi_config_t cfg = {};
                    std::memcpy(cfg.sta.ssid, s_ssid, std::min(std::strlen(s_ssid), sizeof(cfg.sta.ssid)));
                    std::memcpy(cfg.sta.password, s_pass, std::min(std::strlen(s_pass), sizeof(cfg.sta.password)));
                    cfg.sta.bssid_set = false;
                    cfg.sta.channel   = 0;  // scan all channels
                    cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
                    cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;

                    esp_wifi_set_config(WIFI_IF_STA, &cfg);
                }
                esp_wifi_connect();
            }
            else
            {
                ESP_LOGE(kTag, "Max retries reached");
                set_state(rs520::WifiState::kDisconnected);
                xEventGroupSetBits(s_event_group, kBitFail);
            }
            break;
        }

        case WIFI_EVENT_STA_CONNECTED:
        {
            auto* ev = static_cast<wifi_event_sta_connected_t*>(event_data);
            ESP_LOGI(kTag, "Associated with AP, channel=%d", ev->channel);
            // Save BSSID + channel for fast reconnect next boot
            nvs_save_bssid_channel(ev->bssid, ev->channel);
            break;
        }

        default:
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        auto* ev = static_cast<ip_event_got_ip_t*>(event_data);
        ESP_LOGI(kTag, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        s_retry_count = 0;
        set_state(rs520::WifiState::kConnected);
        xEventGroupSetBits(s_event_group, kBitConnected);
    }
}

// --- Roaming task ---

static void roaming_task(void* /*arg*/)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(kRoamIntervalMs));

        if (s_state != rs520::WifiState::kConnected) continue;

        // Get current AP info
        wifi_ap_record_t current = {};
        if (esp_wifi_sta_get_ap_info(&current) != ESP_OK) continue;

        // Passive scan (non-blocking, doesn't disconnect)
        wifi_scan_config_t scan_cfg = {};
        scan_cfg.ssid = reinterpret_cast<uint8_t*>(s_ssid);
        scan_cfg.scan_type = WIFI_SCAN_TYPE_PASSIVE;
        scan_cfg.scan_time.passive = 120;  // ms per channel

        if (esp_wifi_scan_start(&scan_cfg, true) != ESP_OK) continue;

        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        if (ap_count == 0)
        {
            esp_wifi_clear_ap_list();
            continue;
        }

        // Cap to avoid large stack allocation
        constexpr uint16_t kMaxAps = 10;
        wifi_ap_record_t ap_list[kMaxAps];
        uint16_t fetched = std::min(ap_count, kMaxAps);
        esp_wifi_scan_get_ap_records(&fetched, ap_list);

        // Find strongest AP with matching SSID
        int8_t best_rssi = current.rssi;
        int best_idx = -1;
        for (uint16_t i = 0; i < fetched; i++)
        {
            if (std::strcmp(reinterpret_cast<const char*>(ap_list[i].ssid),
                           reinterpret_cast<const char*>(current.ssid)) != 0)
            {
                continue;
            }
            // Skip current AP
            if (std::memcmp(ap_list[i].bssid, current.bssid, 6) == 0)
            {
                continue;
            }
            if (ap_list[i].rssi > best_rssi + kRoamHysteresis)
            {
                best_rssi = ap_list[i].rssi;
                best_idx  = i;
            }
        }

        if (best_idx >= 0)
        {
            ESP_LOGI(kTag, "Roaming: current RSSI=%d, better AP RSSI=%d",
                     current.rssi, ap_list[best_idx].rssi);

            wifi_config_t cfg = {};
            std::memcpy(cfg.sta.ssid, s_ssid, std::min(std::strlen(s_ssid), sizeof(cfg.sta.ssid)));
            std::memcpy(cfg.sta.password, s_pass, std::min(std::strlen(s_pass), sizeof(cfg.sta.password)));
            cfg.sta.bssid_set = true;
            std::memcpy(cfg.sta.bssid, ap_list[best_idx].bssid, 6);
            cfg.sta.channel = ap_list[best_idx].primary;
            cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

            esp_wifi_disconnect();
            esp_wifi_set_config(WIFI_IF_STA, &cfg);
            esp_wifi_connect();
        }
    }
}

}  // namespace

namespace rs520
{

esp_err_t wifi_init()
{
    s_event_group = xEventGroupCreate();
    if (!s_event_group) return ESP_ERR_NO_MEM;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();

    // Set hostname before WiFi starts
    build_hostname();
    esp_netif_set_hostname(s_sta_netif, s_hostname);
    ESP_LOGI(kTag, "Hostname: %s", s_hostname);

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                wifi_event_handler, nullptr));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));  // No power save — low latency

    return ESP_OK;
}

esp_err_t wifi_connect()
{
    esp_err_t ret = nvs_load_credentials();
    if (ret != ESP_OK)
    {
        ESP_LOGW(kTag, "No stored credentials");
        return ret;
    }

    ESP_LOGI(kTag, "Connecting to SSID: %s (has_bssid=%d, chan=%d)",
             s_ssid, s_has_bssid, s_channel);

    s_retry_count = 0;
    set_state(WifiState::kConnecting);

    wifi_config_t cfg = {};
    std::memcpy(cfg.sta.ssid, s_ssid, std::min(std::strlen(s_ssid), sizeof(cfg.sta.ssid)));
    std::memcpy(cfg.sta.password, s_pass, std::min(std::strlen(s_pass), sizeof(cfg.sta.password)));

    // Fast connect: use stored BSSID + channel to skip scan
    if (s_has_bssid && s_channel > 0)
    {
        ESP_LOGI(kTag, "Fast connect: BSSID=%02x:%02x:%02x:%02x:%02x:%02x ch=%d",
                 s_bssid[0], s_bssid[1], s_bssid[2],
                 s_bssid[3], s_bssid[4], s_bssid[5], s_channel);

        cfg.sta.bssid_set = true;
        std::memcpy(cfg.sta.bssid, s_bssid, 6);
        cfg.sta.channel = s_channel;
        s_fast_connect = true;
    }
    else
    {
        // Full scan, connect to strongest
        cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
        cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        s_fast_connect = false;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection result
    EventBits_t bits = xEventGroupWaitBits(s_event_group,
                                            kBitConnected | kBitFail,
                                            pdTRUE, pdFALSE,
                                            pdMS_TO_TICKS(15000));

    if (bits & kBitConnected)
    {
        ESP_LOGI(kTag, "Connected successfully");
        // Start roaming task
        xTaskCreate(roaming_task, "wifi_roam", kRoamTaskStack, nullptr,
                    kRoamTaskPrio, nullptr);
        return ESP_OK;
    }

    ESP_LOGE(kTag, "Connection failed");
    return ESP_FAIL;
}

esp_err_t wifi_disconnect()
{
    set_state(WifiState::kDisconnected);
    return esp_wifi_disconnect();
}

WifiState wifi_state()
{
    return s_state;
}

int8_t wifi_rssi()
{
    if (s_state != WifiState::kConnected) return 0;

    wifi_ap_record_t info = {};
    if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) return 0;
    return info.rssi;
}

void wifi_on_state_change(WifiStateCallback cb, void* ctx)
{
    s_state_cb  = cb;
    s_state_ctx = ctx;
}

void wifi_set_state(WifiState state)
{
    set_state(state);
}

esp_err_t wifi_clear_credentials()
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(kNvsNamespace, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_erase_key(h, kNvsKeySsid);
    nvs_erase_key(h, kNvsKeyPass);
    nvs_erase_key(h, kNvsKeyBssid);
    nvs_erase_key(h, kNvsKeyChan);
    nvs_commit(h);
    nvs_close(h);

    ESP_LOGI(kTag, "Credentials cleared");
    return ESP_OK;
}

esp_err_t wifi_store_credentials(const char* ssid, const char* password)
{
    if (!ssid || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(kNvsNamespace, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    nvs_set_str(h, kNvsKeySsid, ssid);
    nvs_set_str(h, kNvsKeyPass, password ? password : "");
    // Clear old BSSID/channel — will be learned on first connect
    nvs_erase_key(h, kNvsKeyBssid);
    nvs_erase_key(h, kNvsKeyChan);
    nvs_commit(h);
    nvs_close(h);

    ESP_LOGI(kTag, "Credentials stored for SSID: %s", ssid);
    return ESP_OK;
}

bool wifi_wait_connected(int timeout_ms)
{
    if (s_state == WifiState::kConnected) return true;
    if (!s_event_group) return false;

    EventBits_t bits = xEventGroupWaitBits(s_event_group,
                                            kBitConnected,
                                            pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(timeout_ms));
    return (bits & kBitConnected) != 0;
}

}  // namespace rs520
