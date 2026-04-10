#include "battery.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cmath>

namespace
{

constexpr const char* kTag = "battery";

// Hardware config (docs/esp/BATTERY_MONITORING.md)
constexpr adc_channel_t    kAdcChannel = ADC_CHANNEL_0;        // GPIO 1
constexpr adc_atten_t      kAdcAtten   = ADC_ATTEN_DB_12;     // 0–3.3V range
constexpr adc_unit_t       kAdcUnit    = ADC_UNIT_1;           // ADC1 (WiFi-safe)
constexpr float            kDividerRatio = 2.0f;               // 10kΩ/10kΩ
constexpr int              kSampleCount  = 16;                 // Average 16 readings
constexpr int              kSampleDelayMs = 1;                 // 1ms between samples

// Monitoring
constexpr uint32_t kMonitorIntervalMs = 30000;   // 30 seconds
constexpr int      kMonitorTaskStack  = 2560;
constexpr int      kMonitorTaskPrio   = 3;

// State thresholds (with hysteresis)
constexpr int kThreshLow       = 10;   // Enter low at <= 10%
constexpr int kThreshLowExit   = 12;   // Exit low at >= 12%
constexpr int kThreshCritical  = 5;    // Enter critical at <= 5%
constexpr int kThreshCritExit  = 7;    // Exit critical at >= 7%
constexpr float kChargingVoltage = 4.15f;  // Above this = USB/charging

// LiPo discharge curve (voltage → percentage)
// 14 points from docs/esp/BATTERY_MONITORING.md
struct CurvePoint { float voltage; int percent; };
constexpr CurvePoint kDischargeCurve[] = {
    {4.20f, 100},
    {4.15f,  95},
    {4.10f,  90},
    {4.05f,  85},
    {4.00f,  80},
    {3.95f,  75},
    {3.90f,  70},
    {3.85f,  60},
    {3.80f,  55},
    {3.75f,  50},
    {3.70f,  40},
    {3.65f,  30},
    {3.60f,  20},
    {3.00f,   0},
};
constexpr int kCurveLen = sizeof(kDischargeCurve) / sizeof(kDischargeCurve[0]);

// ADC handles
static adc_oneshot_unit_handle_t s_adc_handle = nullptr;
static adc_cali_handle_t         s_cali_handle = nullptr;

// Cached readings (updated by monitor task)
static float                     s_voltage    = 0.0f;
static int                       s_percentage = 0;
static rs520::BatteryState       s_state      = rs520::BatteryState::kNormal;

// Callback
static rs520::BatteryStateCallback s_callback = nullptr;
static void*                       s_cb_ctx   = nullptr;

/// Interpolate percentage from voltage using discharge curve
static int voltage_to_percent(float voltage)
{
    if (voltage >= kDischargeCurve[0].voltage) return 100;
    if (voltage <= kDischargeCurve[kCurveLen - 1].voltage) return 0;

    for (int i = 0; i < kCurveLen - 1; ++i)
    {
        float v_hi = kDischargeCurve[i].voltage;
        float v_lo = kDischargeCurve[i + 1].voltage;

        if (voltage <= v_hi && voltage >= v_lo)
        {
            int p_hi = kDischargeCurve[i].percent;
            int p_lo = kDischargeCurve[i + 1].percent;
            float ratio = (voltage - v_lo) / (v_hi - v_lo);
            return p_lo + static_cast<int>(ratio * static_cast<float>(p_hi - p_lo));
        }
    }
    return 0;
}

/// Read ADC, average, apply calibration + divider ratio → voltage
static float read_voltage()
{
    if (!s_adc_handle) return 0.0f;

    int sum = 0;
    int valid = 0;

    for (int i = 0; i < kSampleCount; ++i)
    {
        int raw = 0;
        esp_err_t ret = adc_oneshot_read(s_adc_handle, kAdcChannel, &raw);
        if (ret == ESP_OK)
        {
            sum += raw;
            ++valid;
        }
        vTaskDelay(pdMS_TO_TICKS(kSampleDelayMs));
    }

    if (valid == 0) return 0.0f;

    int avg_raw = sum / valid;
    int voltage_mv = 0;

    if (s_cali_handle)
    {
        adc_cali_raw_to_voltage(s_cali_handle, avg_raw, &voltage_mv);
    }
    else
    {
        // Fallback: linear conversion
        voltage_mv = (avg_raw * 3300) / 4095;
    }

    // Apply voltage divider ratio
    return static_cast<float>(voltage_mv) * kDividerRatio / 1000.0f;
}

/// Derive state from percentage and voltage, with hysteresis
static rs520::BatteryState compute_state(float voltage, int percent, rs520::BatteryState prev)
{
    // Charging takes priority
    if (voltage > kChargingVoltage)
    {
        return rs520::BatteryState::kCharging;
    }

    // Apply hysteresis based on previous state
    switch (prev)
    {
    case rs520::BatteryState::kCritical:
        // Stay critical until >= exit threshold
        if (percent >= kThreshCritExit) break;
        return rs520::BatteryState::kCritical;

    case rs520::BatteryState::kLow:
        // Drop to critical?
        if (percent <= kThreshCritical) return rs520::BatteryState::kCritical;
        // Stay low until >= exit threshold
        if (percent >= kThreshLowExit) break;
        return rs520::BatteryState::kLow;

    default:
        break;
    }

    // Fresh evaluation (from normal/charging or exited hysteresis band)
    if (percent <= kThreshCritical) return rs520::BatteryState::kCritical;
    if (percent <= kThreshLow)     return rs520::BatteryState::kLow;
    return rs520::BatteryState::kNormal;
}

/// Monitor task: sample voltage, update state, fire callback on transitions
static void monitor_task(void* /*arg*/)
{
    // Initial reading immediately
    for (;;)
    {
        float voltage = read_voltage();
        int percent   = voltage_to_percent(voltage);

        s_voltage    = voltage;
        s_percentage = percent;

        auto new_state = compute_state(voltage, percent, s_state);

        ESP_LOGI(kTag, "Battery: %.2fV  %d%%  state=%d", voltage, percent,
                 static_cast<int>(new_state));

        if (new_state != s_state)
        {
            auto old = s_state;
            s_state = new_state;
            ESP_LOGW(kTag, "Battery state: %d -> %d", static_cast<int>(old),
                     static_cast<int>(new_state));

            if (s_callback)
            {
                s_callback(new_state, percent, s_cb_ctx);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kMonitorIntervalMs));
    }
}

/// Try curve-fitting calibration (ESP32-S3 preferred), fallback to line-fitting
static esp_err_t init_calibration()
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {};
    cali_cfg.unit_id  = kAdcUnit;
    cali_cfg.atten    = kAdcAtten;
    cali_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;

    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
    if (ret == ESP_OK)
    {
        ESP_LOGI(kTag, "ADC calibration: curve fitting");
        return ESP_OK;
    }
    ESP_LOGW(kTag, "Curve fitting failed: %s, trying line fitting", esp_err_to_name(ret));
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t line_cfg = {};
    line_cfg.unit_id  = kAdcUnit;
    line_cfg.atten    = kAdcAtten;
    line_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;

    esp_err_t ret2 = adc_cali_create_scheme_line_fitting(&line_cfg, &s_cali_handle);
    if (ret2 == ESP_OK)
    {
        ESP_LOGI(kTag, "ADC calibration: line fitting");
        return ESP_OK;
    }
    ESP_LOGW(kTag, "Line fitting failed: %s, using raw conversion", esp_err_to_name(ret2));
#endif

    ESP_LOGW(kTag, "No ADC calibration available — using raw linear conversion");
    return ESP_ERR_NOT_SUPPORTED;
}

}  // namespace

namespace rs520
{

esp_err_t battery_init()
{
    ESP_LOGI(kTag, "Initializing battery monitor (ADC1_CH0, GPIO 1)");

    // Configure ADC oneshot unit
    adc_oneshot_unit_init_cfg_t unit_cfg = {};
    unit_cfg.unit_id = kAdcUnit;

    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &s_adc_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "ADC unit init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure channel
    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.atten    = kAdcAtten;
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;

    ret = adc_oneshot_config_channel(s_adc_handle, kAdcChannel, &chan_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(kTag, "ADC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Calibration (best-effort — works without it)
    init_calibration();

    // Start monitoring task
    BaseType_t ok = xTaskCreate(monitor_task, "battery", kMonitorTaskStack,
                                nullptr, kMonitorTaskPrio, nullptr);
    if (ok != pdTRUE)
    {
        ESP_LOGE(kTag, "Failed to create battery monitor task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag, "Battery monitor started (every %lu ms)",
             static_cast<unsigned long>(kMonitorIntervalMs));
    return ESP_OK;
}

float battery_voltage()
{
    return s_voltage;
}

int battery_percentage()
{
    return s_percentage;
}

BatteryState battery_state()
{
    return s_state;
}

bool battery_is_charging()
{
    return s_voltage > kChargingVoltage;
}

void battery_on_state_change(BatteryStateCallback cb, void* ctx)
{
    s_callback = cb;
    s_cb_ctx   = ctx;
}

}  // namespace rs520
