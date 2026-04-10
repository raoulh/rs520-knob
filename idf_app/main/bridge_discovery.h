#ifndef RS520_BRIDGE_DISCOVERY_H
#define RS520_BRIDGE_DISCOVERY_H

#include "esp_err.h"
#include <cstdint>

namespace rs520
{

/// Bridge connection state
enum class BridgeState : uint8_t
{
    kDisconnected,
    kSearching,
    kConnecting,
    kConnected,
};

/// Callback for bridge state changes.
/// Called from WS event task — do NOT call LVGL directly. Use lv_async_call().
using BridgeStateCallback = void (*)(BridgeState state, void* ctx);

/// Initialize bridge discovery. Starts mDNS browse + WS connect task.
/// Must be called after wifi_init() when WiFi is connected.
[[nodiscard]] esp_err_t bridge_discovery_init();

/// Get current bridge connection state.
BridgeState bridge_state();

/// Check if bridge WebSocket is connected.
bool bridge_is_connected();

/// Register callback for state transitions. Only one callback supported.
void bridge_on_state_change(BridgeStateCallback cb, void* ctx);

/// Send volume command to bridge (throttled at 30ms).
/// Non-blocking: stores pending value, coalesces rapid calls.
void bridge_send_volume(int volume);

}  // namespace rs520

#endif // RS520_BRIDGE_DISCOVERY_H
