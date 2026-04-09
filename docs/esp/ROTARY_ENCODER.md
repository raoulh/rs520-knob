# Rotary Encoder Input

How the firmware reads the physical knob (rotary encoder) and translates rotation into volume control for the RS520.

## Hardware Overview

| Component | Type | Interface |
|-----------|------|-----------|
| Encoder | Incremental quadrature | GPIO |
| Channel A | GPIO 8 | Input with pull-up |
| Channel B | GPIO 7 | Input with pull-up |

**Note:** This encoder has NO push button. The shaft is rotation-only. All button-like interactions use the touchscreen.

## Quadrature Encoding

The encoder produces two signals, 90° out of phase:

```
Clockwise rotation:
Channel A:  ──┐   ┌───┐   ┌───
              └───┘   └───┘
Channel B:  ────┐   ┌───┐   ┌─
                └───┘   └───┘

Counter-clockwise:
Channel A:  ────┐   ┌───┐   ┌─
                └───┘   └───┘
Channel B:  ──┐   ┌───┐   ┌───
              └───┘   └───┘
```

Direction detection:
- **A rises before B** → clockwise (volume up)
- **B rises before A** → counter-clockwise (volume down)

## GPIO Configuration

```cpp
gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << kEncoderGpioA),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,  // Poll, no interrupts
};
gpio_config(&io_conf);
```

Internal pull-ups active. Encoder contacts connect to ground when closed:
- **High (1)** = contact open
- **Low (0)** = contact closed

## Debouncing

Mechanical encoders produce contact bounce. The firmware uses a debounce counter:

```cpp
struct EncoderState {
    uint8_t debounce_a_cnt{};
    uint8_t debounce_b_cnt{};
    uint8_t encoder_a_level{};  // Last stable level
    uint8_t encoder_b_level{};
    int count_value{};          // Accumulated rotation count
};

constexpr int kDebounceTicks = 2;  // Must see same level 2× to accept
```

## Polling

A 3ms periodic timer polls both channels:

```cpp
constexpr int kPollIntervalMs = 3;

static void poll_timer_cb(void* arg) {
    encoder_read_and_dispatch();
}

esp_timer_start_periodic(poll_timer, kPollIntervalMs * 1000);
```

3ms is fast enough for reasonable rotation speeds (~300 RPM max).

## Event Dispatch

When count changes, an event is queued for the UI:

```cpp
static void encoder_read_and_dispatch() {
    // ... polling and decoding ...

    int delta = encoder.count_value - last_count;
    if (delta != 0) {
        last_count = encoder.count_value;

        auto input = (delta > 0) ? InputEvent::kVolUp : InputEvent::kVolDown;

        // Timer callback runs in interrupt context — use ISR-safe queue
        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(input_queue, &input, &woken);
        if (woken) {
            portYIELD_FROM_ISR();
        }
    }
}
```

## Alternative: Hardware PCNT

The ESP32-S3 has a Pulse Counter (PCNT) peripheral for hardware quadrature decoding:

- 4× mode with 1–2µs glitch filter
- Interrupt on count threshold
- Less CPU than software polling

Software polling is simpler and works reliably. Consider PCNT if you need higher resolution or lower CPU usage.

## Tuning

| Parameter | Default | Notes |
|-----------|---------|-------|
| Poll interval | 3ms | Lower = snappier, higher = less CPU |
| Debounce ticks | 2 | Increase if noisy |
| Direction swap | — | Flip A/B mapping if rotation feels inverted |

## Related Docs

- [Encoder Hardware Reference](hw-reference/encoder.md) — hardware notes
- [Hardware Pins](hw-reference/HARDWARE_PINS.md) — GPIO assignments
- [Touch Input](TOUCH_INPUT.md) — touchscreen complements encoder (no push button)

## Current Implementation

Source: `idf_app/main/encoder.h` / `encoder.cpp`

- **Namespace**: `rs520`
- **GPIO**: A=8, B=7, pull-up enabled, no interrupts
- **Debounce**: 2-tick threshold per channel
- **Poll**: 3ms `esp_timer` periodic callback (ISR context)
- **Output**: `FreeRTOS` queue of `EncoderDir` enum (`kCW`/`kCCW`)
- **Consumer**: dedicated task reads queue → updates LVGL progress arc + fires DRV2605 haptic click
- **Bounds**: progress clamped 0–100%, no haptic at limits

### Integration with Haptic + UI

```
encoder_task loop:
  xQueueReceive(encoder_queue) → EncoderDir
  if (next value out of [0,100]) → skip
  lvgl_port_lock → progress_ui_adjust(±1) → lvgl_port_unlock
  haptic_click()
```
