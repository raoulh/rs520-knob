# Battery Monitoring

Battery voltage monitoring and percentage calculation for the Waveshare ESP32-S3-Knob-Touch-LCD-1.8.

## Hardware Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| Battery type | LiPo 3.7V nominal | 800mAh, model 102035 |
| Connector | PH1.25 2-pin | Included with board |
| Voltage range | 3.0V – 4.2V | 3.0V cutoff, 4.2V full |
| ADC Unit | ADC1 | Required — ADC2 incompatible with WiFi |
| ADC Channel | Channel 0 (GPIO 1) | Verified |
| Attenuation | 12 dB | 0–3.3V input range |
| Resolution | 12-bit | 0–4095 raw values |
| Voltage Divider | 2:1 (10kΩ / 10kΩ) | Verified on hardware |

## Voltage Divider

Battery voltage (up to 4.2V) exceeds ADC 3.3V max. A 2:1 divider scales it:

```
Battery+ ──[10kΩ]──┬──[10kΩ]── GND
                    │
                  GPIO 1 (ADC)
```

- ADC reads ~2.1V max (4.2V ÷ 2)
- When USB connected: reads charging circuit voltage (~4.9V ÷ 2 = ~2.45V)

```cpp
constexpr float kDividerRatio = 2.0f;

float adc_voltage = (static_cast<float>(raw) / 4095.0f) * 3.3f;
float battery_voltage = adc_voltage * kDividerRatio;
```

## LiPo Discharge Curve

Non-linear discharge — use lookup table with linear interpolation:

| Voltage | Percentage | Notes |
|---------|------------|-------|
| 4.20V | 100% | Fully charged |
| 4.15V | 95% | |
| 4.10V | 90% | |
| 4.00V | 80% | |
| 3.90V | 70% | |
| 3.80V | 60% | |
| 3.75V | 50% | Nominal region |
| 3.70V | 40% | |
| 3.65V | 30% | |
| 3.60V | 20% | Low battery warning |
| 3.50V | 10% | Critical |
| 3.30V | 5% | |
| 3.00V | 0% | Cutoff |

## ADC Sampling

Average 16 samples to reduce noise (~16ms total):

```cpp
constexpr int kNumSamples = 16;

int raw_sum = 0;
for (int i = 0; i < kNumSamples; i++) {
    int raw{};
    adc_oneshot_read(adc_handle, kBatteryChannel, &raw);
    raw_sum += raw;
    vTaskDelay(pdMS_TO_TICKS(1));
}
int raw_avg = raw_sum / kNumSamples;
```

## ADC Calibration

ESP-IDF curve-fitting calibration compensates for non-linearity:

```cpp
// Calibrated (preferred)
adc_cali_raw_to_voltage(cali_handle, raw_avg, &voltage_mv);

// Fallback
voltage_mv = (raw_avg * 3300) / 4095;
```

## API

```cpp
// Initialize ADC and calibration
auto battery_init() -> bool;

// Battery voltage (3.0–4.2V range, 0.0 on error)
auto battery_get_voltage() -> float;

// Battery percentage (0–100, interpolated from discharge curve)
auto battery_get_percentage() -> int;

// Heuristic: voltage > 4.15V indicates USB power
auto battery_is_charging() -> bool;
```

## Brownout Detection

```
CONFIG_ESP_BROWNOUT_DET=y
CONFIG_ESP_BROWNOUT_DET_LVL_SEL_4=y
```

Level 4 (2.50V) — safe for battery operation. Default Level 7 (2.80V) triggers false brownouts during WiFi TX spikes.

## Important Notes

- **ADC1 only** — ADC2 cannot be used when WiFi is active
- **USB detection** — when USB connected, ADC reads charging circuit (~4.9V), not battery directly
- **Smoothing** — consider exponential moving average for UI display to avoid jitter

## Related Docs

- [Battery Hardware Reference](hw-reference/battery.md) — charging IC, battery specs
- [Hardware Pins](hw-reference/HARDWARE_PINS.md) — ADC pin assignment
