# Roadmap Ideas

Ideas for future work. Move to GitHub Issues when ready to implement.

## Format

```
### [Idea Title]
**Priority**: High / Medium / Low
**Effort**: Small / Medium / Large
**Notes**: Brief description
```

## Ideas

### mDNS Bridge Discovery
**Priority**: High
**Effort**: Small
**Notes**: `espressif/mdns` managed component is already added. Need to call `mdns_init()` + `mdns_hostname_set()` after IP acquired in `wifi_manager.cpp`. Then browse for `_rs520bridge._tcp.local` to auto-discover bridge IP. Currently bridge address would need to be hardcoded or stored in NVS.

### Long-Press Encoder to Reset WiFi Credentials
**Priority**: Medium
**Effort**: Small
**Notes**: Hold encoder button for 3s at boot → call `wifi_clear_credentials()` → enter provisioning. Need encoder button GPIO + debounce logic in boot sequence.

### WiFi Provisioning — Bridge Address Entry
**Priority**: Medium
**Effort**: Small
**Notes**: Add bridge IP/port field to captive portal HTML form. Store in NVS alongside WiFi creds. Currently the captive portal only handles SSID + password.

### WPA2-Secured Provisioning AP
**Priority**: Low
**Effort**: Small
**Notes**: Currently SoftAP is open (no password). Could add WPA2 with a printed-on-device password for security-conscious deployments. Low risk since provisioning is temporary and local-only.

### QR Code on Provisioning Screen
**Priority**: Low
**Effort**: Medium
**Notes**: Show QR code on LVGL display encoding `WIFI:S:RS520-Knob-XXYYZZ;T:nopass;;` so phones can auto-connect without manual AP selection. LVGL has QR code widget support.

<!-- Add ideas below -->
