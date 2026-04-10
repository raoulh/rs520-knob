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

### ~~mDNS Bridge Discovery~~ — ✅ DONE
**Priority**: High
**Effort**: Small
**Notes**: Implemented in `bridge_discovery.cpp`. ESP32 calls `mdns_query_ptr("_rs520bridge", "_tcp")` to auto-discover bridge. Go bridge advertises via `hashicorp/mdns`. WebSocket client connects automatically. Volume throttle (30ms) + dual-arc UI included. **NVS bridge cache added** — stores IP:port after mDNS success, skips discovery on subsequent boots (~5s saved).

### ~~Boot Time Optimization~~ — ✅ DONE
**Priority**: High
**Effort**: Medium
**Notes**: Parallel WiFi init (background `net_task`), NVS bridge cache, mDNS timeout 5s→3s. Boot-to-usable: 10.6s → 3.6s (66% faster). Event group sync gate prevents LVGL race in provisioning path.

### Long-Press Encoder to Reset WiFi Credentials
**Priority**: Medium
**Effort**: Small
**Notes**: Hold encoder button for 3s at boot → call `wifi_clear_credentials()` → enter provisioning. Need encoder button GPIO + debounce logic in boot sequence.

### WiFi Provisioning — Bridge Address Entry
**Priority**: Low
**Effort**: Small
**Notes**: Add bridge IP/port field to captive portal HTML form. Store in NVS alongside WiFi creds. Currently the captive portal only handles SSID + password. Lower priority now since NVS bridge cache + mDNS auto-discovery covers most cases.

### WPA2-Secured Provisioning AP
**Priority**: Low
**Effort**: Small
**Notes**: Currently SoftAP is open (no password). Could add WPA2 with a printed-on-device password for security-conscious deployments. Low risk since provisioning is temporary and local-only.

### QR Code on Provisioning Screen
**Priority**: Low
**Effort**: Medium
**Notes**: Show QR code on LVGL display encoding `WIFI:S:RS520-Knob-XXYYZZ;T:nopass;;` so phones can auto-connect without manual AP selection. LVGL has QR code widget support.

<!-- Add ideas below -->
