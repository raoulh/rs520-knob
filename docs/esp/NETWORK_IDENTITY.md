# Network Identity

Making the ESP32-S3 knob identifiable on the local network for bridge discovery and router visibility.

## Status

**Implemented** in `wifi_manager.cpp` + `bridge_discovery.cpp`.
- DHCP hostname set during `wifi_init()` before `esp_wifi_start()`
- mDNS initialized in `bridge_discovery_init()` after WiFi connects
- Bridge auto-discovered via `_rs520bridge._tcp` mDNS browse

## Problem

ESP32 devices appear as "espressif" or "Unknown" in router client lists, making them hard to identify and debug.

## Solution

Set both DHCP hostname and mDNS hostname:

```cpp
// 1. DHCP hostname BEFORE WiFi starts
esp_netif_t* netif = esp_netif_create_default_wifi_sta();
esp_netif_set_hostname(netif, "rs520-knob-abc123");

// 2. WiFi init + start
esp_wifi_init(&wifi_config);
esp_wifi_start();

// 3. mDNS AFTER IP acquired
mdns_init();
mdns_hostname_set("rs520-knob-abc123");
```

**Result:**
- Router shows: "rs520-knob-abc123" in client list
- mDNS: `ping rs520-knob-abc123.local`

## Mechanisms

### DHCP Hostname (Option 12)

- Sent during DHCP DISCOVER/REQUEST
- Visible in router admin UIs
- **Timing:** set AFTER `esp_netif_create_*()` but BEFORE `esp_wifi_start()`

### mDNS / Bonjour

- Multicast DNS for `.local` domain resolution
- Visible to mDNS-aware clients (macOS, Linux, iOS)
- **Timing:** AFTER WiFi connects and IP is acquired

### Consistency

DHCP and mDNS hostnames **must match** to avoid confusion.

## Recommended: MAC-Based Hostname

Unique per device, no user config needed:

```cpp
static char hostname_[32]{};

auto get_hostname() -> const char* {
    if (hostname_[0] != '\0') return hostname_;

    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        snprintf(hostname_, sizeof(hostname_), "rs520-knob");
        return hostname_;
    }

    snprintf(hostname_, sizeof(hostname_), "rs520-knob-%02x%02x%02x",
             mac[3], mac[4], mac[5]);
    return hostname_;
}
```

## Bridge Discovery

**Implemented** in `bridge_discovery.cpp`. The knob auto-discovers the Go bridge via mDNS — no manual configuration needed.

### How It Works

1. `bridge_discovery_init()` spawns a FreeRTOS task (4096 stack, prio 4)
2. Task calls `mdns_init()` + `mdns_hostname_set("rs520-knob-XXYYZZ")` (MAC-based)
3. Queries `mdns_query_ptr("_rs520bridge", "_tcp", 5000, ...)` in a loop
4. On match → extracts IP + port from result → connects WebSocket `ws://host:port/ws`
5. On disconnect → retries discovery with 3s backoff

### Bridge Side (Go)

The bridge advertises via `hashicorp/mdns`:
- **Service**: `_rs520bridge._tcp`
- **TXT records**: `version=0.1.0`, `rs520=<RS520_HOST>`
- Verify: `avahi-browse -r _rs520bridge._tcp`

### State Machine

`BridgeState` enum: `kDisconnected` → `kSearching` → `kConnecting` → `kConnected`

State changes fire a callback (registered via `bridge_on_state_change()`) which drives the connection UI overlay.

### Fallback Options (Not Yet Implemented)

- **Static:** bridge IP/port stored in NVS
- **SoftAP provisioning:** user configures bridge address during WiFi setup

## Hostname Sanitization

RFC 1123: lowercase alphanumeric + hyphen only.

```cpp
void sanitize_hostname(const char* input, char* output, size_t len) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < len - 1; i++) {
        char c = input[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
            output[j++] = c;
        } else if (c >= 'A' && c <= 'Z') {
            output[j++] = c + 32;  // Lowercase
        } else if (c == ' ' || c == '_') {
            output[j++] = '-';
        }
    }
    output[j] = '\0';
}
```

## WiFi Provisioning (SoftAP Captive Portal)

Implemented in `wifi_provision.cpp`. When no credentials are stored (or connection fails 3 times), the device enters provisioning mode:

1. Starts SoftAP: `RS520-Knob-XXYYZZ` (MAC-based, open, channel 1, max 1 client)
2. DNS server redirects all queries to `192.168.4.1`
3. HTTP server serves captive portal form at `/`
4. User selects SSID from scan results + enters password
5. Credentials stored in NVS → device switches to STA mode

### Captive Portal Routes

| Route | Method | Purpose |
|-------|--------|---------|
| `/` | GET | HTML form (SSID dropdown + password) |
| `/scan` | GET | JSON array of visible APs |
| `/connect` | POST | Store creds, start STA connect |
| `/status` | GET | Connection result polling |
| `/*` | GET | 302 redirect to `/` (captive portal trigger) |

### NVS Credential Storage

Namespace: `wifi`

| Key | Type | Purpose |
|-----|------|---------|
| `ssid` | string | WiFi SSID (max 32 chars) |
| `pass` | string | WiFi password (max 64 chars) |
| `bssid` | blob (6B) | AP MAC for fast reconnect |
| `channel` | uint8 | AP channel for fast reconnect |

## Fast Reconnect

On successful connection, `wifi_manager.cpp` stores the AP's BSSID and channel in NVS. On next boot, the device connects directly using `bssid_set=true` + specific channel — skipping the scan phase entirely. This reduces connection time to ~500ms vs 2–3s with full scan.

If fast connect fails (AP moved/replaced), falls back to full all-channel scan sorted by RSSI.

## AP Roaming

Roaming task runs every 60s when connected. Does passive scan for same SSID. Switches to stronger AP if signal delta > 8 dBm (hysteresis prevents flapping).

## Related Docs

- [Development](../dev/DEVELOPMENT.md) — WiFi setup and build
- [Hardware Pins](hw-reference/HARDWARE_PINS.md) — GPIO reference
