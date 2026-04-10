#include "bridge_discovery.h"
#include "progress_ui.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "mdns.h"
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace
{

constexpr const char* kTag = "bridge";
constexpr const char* kServiceType = "_rs520bridge._tcp";
constexpr int kDiscoveryTaskStack  = 4096;
constexpr int kDiscoveryTaskPrio   = 4;
constexpr int kMdnsTimeoutMs       = 5000;
constexpr int kRetryDelayMs        = 3000;
constexpr int kThrottleUs          = 30000;  // 30ms

// State
static rs520::BridgeState s_state = rs520::BridgeState::kDisconnected;
static rs520::BridgeStateCallback s_cb = nullptr;
static void* s_cb_ctx = nullptr;

// WebSocket client handle
static esp_websocket_client_handle_t s_ws_client = nullptr;

// Volume throttle
static esp_timer_handle_t s_throttle_timer = nullptr;
static std::atomic<int> s_pending_volume{-1};  // -1 = nothing pending

static void set_state(rs520::BridgeState new_state)
{
    if (s_state == new_state) return;
    s_state = new_state;
    ESP_LOGI(kTag, "state → %d", static_cast<int>(new_state));
    if (s_cb)
    {
        s_cb(new_state, s_cb_ctx);
    }
}

// --- Volume throttle timer callback ---
static void throttle_timer_cb(void* /*arg*/)
{
    int vol = s_pending_volume.exchange(-1);
    if (vol < 0) return;
    if (!s_ws_client || !esp_websocket_client_is_connected(s_ws_client)) return;

    char buf[48];
    int len = snprintf(buf, sizeof(buf), R"({"cmd":"volume","value":%d})", vol);
    esp_websocket_client_send_text(s_ws_client, buf, len, pdMS_TO_TICKS(100));
}

// --- Incoming WS event parser ---

static void handle_ws_data(const char* data, int len)
{
    cJSON* root = cJSON_ParseWithLength(data, len);
    if (!root) return;

    cJSON* evt = cJSON_GetObjectItem(root, "evt");
    if (!evt || !cJSON_IsString(evt))
    {
        cJSON_Delete(root);
        return;
    }

    const char* evt_str = evt->valuestring;

    if (strcmp(evt_str, "state") == 0)
    {
        // Full state sync — set both arcs
        cJSON* vol_item = cJSON_GetObjectItem(root, "volume");
        if (vol_item && cJSON_IsNumber(vol_item))
        {
            int vol = vol_item->valueint;
            lvgl_port_lock(0);
            rs520::progress_ui_set_confirmed(vol);
            rs520::progress_ui_set_target(vol);
            lvgl_port_unlock();
        }
        ESP_LOGI(kTag, "state sync received");
    }
    else if (strcmp(evt_str, "volume") == 0)
    {
        // Volume-only update — move confirmed arc
        cJSON* vol_item = cJSON_GetObjectItem(root, "volume");
        if (vol_item && cJSON_IsNumber(vol_item))
        {
            int vol = vol_item->valueint;
            lvgl_port_lock(0);
            rs520::progress_ui_set_confirmed(vol);
            lvgl_port_unlock();
        }
    }
    else if (strcmp(evt_str, "connected") == 0)
    {
        cJSON* dev = cJSON_GetObjectItem(root, "device");
        if (dev && cJSON_IsString(dev))
        {
            ESP_LOGI(kTag, "amp connected: %s", dev->valuestring);
        }
    }
    else if (strcmp(evt_str, "error") == 0)
    {
        cJSON* title = cJSON_GetObjectItem(root, "title");
        if (title && cJSON_IsString(title))
        {
            ESP_LOGW(kTag, "bridge error: %s", title->valuestring);
        }
    }

    cJSON_Delete(root);
}

// --- WebSocket event handler ---
static void ws_event_handler(void* /*arg*/, esp_event_base_t /*base*/,
                             int32_t event_id, void* event_data)
{
    auto* data = static_cast<esp_websocket_event_data_t*>(event_data);

    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(kTag, "WebSocket connected");
        set_state(rs520::BridgeState::kConnected);
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(kTag, "WebSocket disconnected");
        set_state(rs520::BridgeState::kDisconnected);
        break;

    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x01 && data->data_len > 0)  // text frame
        {
            handle_ws_data(data->data_ptr, data->data_len);
        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(kTag, "WebSocket error");
        break;

    default:
        break;
    }
}

// --- Connect to bridge at given IP:port ---
static bool connect_ws(const char* host, uint16_t port)
{
    // Tear down previous client if any
    if (s_ws_client)
    {
        esp_websocket_client_stop(s_ws_client);
        esp_websocket_client_destroy(s_ws_client);
        s_ws_client = nullptr;
    }

    char uri[96];
    snprintf(uri, sizeof(uri), "ws://%s:%u/ws", host, port);
    ESP_LOGI(kTag, "connecting to %s", uri);

    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri = uri;
    ws_cfg.buffer_size = 1024;
    ws_cfg.reconnect_timeout_ms = kRetryDelayMs;
    ws_cfg.network_timeout_ms = 5000;

    s_ws_client = esp_websocket_client_init(&ws_cfg);
    if (!s_ws_client) return false;

    esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY,
                                  ws_event_handler, nullptr);

    set_state(rs520::BridgeState::kConnecting);
    esp_err_t err = esp_websocket_client_start(s_ws_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "ws start failed: %s", esp_err_to_name(err));
        esp_websocket_client_destroy(s_ws_client);
        s_ws_client = nullptr;
        return false;
    }

    return true;
}

// --- Discovery task: mDNS browse → WS connect → retry loop ---
static void discovery_task(void* /*arg*/)
{
    // Initialize mDNS
    ESP_ERROR_CHECK(mdns_init());

    // Get MAC for unique hostname
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char hostname[32];
    snprintf(hostname, sizeof(hostname), "rs520-knob-%02x%02x%02x",
             mac[3], mac[4], mac[5]);
    mdns_hostname_set(hostname);
    ESP_LOGI(kTag, "mDNS hostname: %s.local", hostname);

    for (;;)
    {
        // If already connected, just sleep and re-check
        if (s_ws_client && esp_websocket_client_is_connected(s_ws_client))
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        set_state(rs520::BridgeState::kSearching);
        ESP_LOGI(kTag, "searching for %s ...", kServiceType);

        mdns_result_t* results = nullptr;
        esp_err_t err = mdns_query_ptr(kServiceType, "_tcp", kMdnsTimeoutMs, 4, &results);

        if (err != ESP_OK || !results)
        {
            ESP_LOGW(kTag, "mDNS: no bridge found, retrying in %d ms", kRetryDelayMs);
            if (results) mdns_query_results_free(results);
            vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
            continue;
        }

        // Use first result with an IPv4 address
        bool connected = false;
        for (auto* r = results; r && !connected; r = r->next)
        {
            if (!r->addr) continue;

            // Walk address list for IPv4
            for (auto* a = r->addr; a && !connected; a = a->next)
            {
                if (a->addr.type == ESP_IPADDR_TYPE_V4)
                {
                    char ip[16];
                    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&a->addr.u_addr.ip4));
                    ESP_LOGI(kTag, "found bridge at %s:%u", ip, r->port);
                    connected = connect_ws(ip, r->port);
                }
            }
        }

        mdns_query_results_free(results);

        if (!connected)
        {
            set_state(rs520::BridgeState::kDisconnected);
            vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
        }
        else
        {
            // Wait a bit before checking again (auto-reconnect handles short drops)
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

}  // namespace

namespace rs520
{

esp_err_t bridge_discovery_init()
{
    // Create volume throttle timer (one-shot, 30ms)
    const esp_timer_create_args_t timer_args = {
        .callback = throttle_timer_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "vol_throttle",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_throttle_timer));

    // Start discovery task
    xTaskCreate(discovery_task, "bridge", kDiscoveryTaskStack, nullptr,
                kDiscoveryTaskPrio, nullptr);

    return ESP_OK;
}

BridgeState bridge_state()
{
    return s_state;
}

bool bridge_is_connected()
{
    return s_state == BridgeState::kConnected;
}

void bridge_on_state_change(BridgeStateCallback cb, void* ctx)
{
    s_cb = cb;
    s_cb_ctx = ctx;
}

void bridge_send_volume(int volume)
{
    s_pending_volume.store(volume);

    // Restart one-shot timer. If already running, stop + restart.
    esp_timer_stop(s_throttle_timer);  // ignore error if not running
    esp_timer_start_once(s_throttle_timer, kThrottleUs);
}

}  // namespace rs520
