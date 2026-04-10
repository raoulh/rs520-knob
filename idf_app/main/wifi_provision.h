#ifndef RS520_WIFI_PROVISION_H
#define RS520_WIFI_PROVISION_H

#include "esp_err.h"

namespace rs520
{

/// Start SoftAP captive portal for WiFi provisioning.
/// Starts AP, DNS redirect, HTTP server with credential form.
/// On successful credential submission: stores NVS, stops provision, starts STA.
[[nodiscard]] esp_err_t provision_start();

/// Stop provisioning (tear down AP, HTTP, DNS).
esp_err_t provision_stop();

/// True if provisioning is currently active.
bool provision_active();

/// Get AP SSID used for provisioning (valid after provision_start).
const char* provision_ssid();

}  // namespace rs520

#endif // RS520_WIFI_PROVISION_H
