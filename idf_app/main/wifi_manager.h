#ifndef RS520_WIFI_MANAGER_H
#define RS520_WIFI_MANAGER_H

#include "esp_err.h"
#include <cstdint>

namespace rs520
{

/// WiFi connection state
enum class WifiState : uint8_t
{
    kDisconnected,
    kConnecting,
    kConnected,
    kProvisioning,
};

/// Callback type for WiFi state changes.
/// Called from WiFi event task — do NOT call LVGL directly. Use lv_async_call().
using WifiStateCallback = void (*)(WifiState state, void* ctx);

/// Initialize WiFi subsystem (netif, event loop, driver).
/// Must be called after nvs_flash_init().
[[nodiscard]] esp_err_t wifi_init();

/// Attempt STA connection using stored NVS credentials.
/// Fast path: stored BSSID+channel (skips scan).
/// Fallback: full scan sorted by RSSI.
/// Returns ESP_OK if connection started, ESP_ERR_NVS_NOT_FOUND if no creds.
[[nodiscard]] esp_err_t wifi_connect();

/// Disconnect and stop WiFi.
esp_err_t wifi_disconnect();

/// Get current WiFi state.
WifiState wifi_state();

/// Get current RSSI (only valid when connected). Returns 0 if not connected.
int8_t wifi_rssi();

/// Register callback for state changes. Only one callback supported.
void wifi_on_state_change(WifiStateCallback cb, void* ctx);

/// Set WiFi state and fire callback. Used by provisioning module.
void wifi_set_state(WifiState state);

/// Erase stored WiFi credentials from NVS.
esp_err_t wifi_clear_credentials();

/// Store WiFi credentials to NVS (called by provisioning).
esp_err_t wifi_store_credentials(const char* ssid, const char* password);

/// Block until WiFi is connected or timeout.
/// Returns true if connected, false on timeout.
bool wifi_wait_connected(int timeout_ms);

}  // namespace rs520

#endif // RS520_WIFI_MANAGER_H
