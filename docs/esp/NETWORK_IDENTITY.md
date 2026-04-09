# Network Identity

Making the ESP32-S3 knob identifiable on the local network for bridge discovery and router visibility.

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

The Go bridge can be discovered via mDNS or configured statically. The knob needs to find the bridge on the local network:

- **mDNS:** bridge advertises a service (e.g., `_rs520bridge._tcp.local`)
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

## Related Docs

- [Development](../dev/DEVELOPMENT.md) — WiFi setup and build
- [Hardware Pins](hw-reference/HARDWARE_PINS.md) — GPIO reference
