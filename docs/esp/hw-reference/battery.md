# Battery Management — Hardware Reference

Hardware details for battery monitoring and charging on the Waveshare ESP32-S3-Knob-Touch-LCD-1.8.

## Battery Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Type | Lithium Polymer (LiPo) | 3.7V nominal |
| Voltage Range | 3.0V – 4.2V | 3.0V cutoff, 4.2V full |
| Capacity | 800mAh | Model: 102035 |
| Connector | PH1.25 2-pin | Included with board |

## ADC Configuration (Verified)

| Parameter | Value | Notes |
|-----------|-------|-------|
| GPIO Pin | GPIO 1 | ADC1_CH0 |
| ADC Unit | ADC1 | Required — ADC2 incompatible with WiFi |
| Resolution | 12-bit | 0–4095 range |
| Attenuation | 12 dB | 0–3.3V input range |

## Voltage Divider (Verified)

```
Battery+ ──[10kΩ]──┬──[10kΩ]── GND
                    │
                  GPIO 1 (ADC)
```

- **R1 (High):** 10kΩ
- **R2 (Low):** 10kΩ
- **Divider Ratio:** 2.0:1
- **Battery 4.2V** → 2.1V at ADC
- **USB ~4.9V** → ~2.45V at ADC (reads charging circuit, not battery)

Verified by: Waveshare demo firmware, BlueKnob project, ESPHome config (all use multiply: 2.0).

## Charging Management

| Parameter | Expected Value | Notes |
|-----------|---------------|-------|
| Charge IC | ETA6096 or similar | On-board |
| Charge Current | ~1A default | Via resistor |
| Charge Cutoff | 4.2V ± 1% | Standard LiPo |
| Charge Input | USB-C 5V | Shared with programming |

## ADC Considerations

1. **Use ADC1 only** — ADC2 cannot be used when WiFi is active
2. **Attenuation:** `ADC_ATTEN_DB_11` for 0–3.3V range
3. **Calibration:** ESP32-S3 two-point calibration for accuracy
4. **Sampling:** Average 16–32 readings to reduce noise
5. **USB detection:** voltage > 4.15V heuristically indicates USB power

## References

- [Battery Monitoring](../BATTERY_MONITORING.md) — firmware implementation
- [Hardware Pins](HARDWARE_PINS.md) — ADC pin assignment
