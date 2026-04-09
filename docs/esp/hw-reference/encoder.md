# Rotary Encoder (EC11-class) — Hardware Notes

Hardware reference for the quadrature rotary encoder on the Waveshare board.

## Overview

- Mechanically-detented quadrature rotary encoder
- Two digital outputs: Channel A (ECA) and Channel B (ECB)
- **Rotation only** — no push switch wired on this board

## Electrical

| Parameter | Value |
|-----------|-------|
| Channel A (ECA) | GPIO 8 |
| Channel B (ECB) | GPIO 7 |
| Contact type | Open-collector (pulled to GND when closed) |
| Pull-ups | Internal (enabled in firmware) |

## Firmware Implementation

- Software quadrature decoder with 3ms polling timer
- Debounce: 2 ticks (state must be stable across two consecutive polls)
- Direction: A leads B → increment (volume up); B leads A → decrement (volume down)
- Events: deltas mapped to volume up/down and queued to UI task

## Alternative: Hardware PCNT

ESP32-S3 PCNT peripheral for hardware quadrature:
- 4× mode with 1–2µs glitch filter
- Interrupt on count threshold
- Less CPU than software polling
- Consider if higher resolution or lower CPU needed

## Tuning

| Parameter | Default | Notes |
|-----------|---------|-------|
| Poll interval | 3ms | Lower = snappier, higher = less CPU |
| Debounce ticks | 2 | Increase if encoder is noisy |
| Direction swap | — | Flip A/B if rotation feels inverted |
| Rate limiting | — | Coalesce deltas to avoid flooding volume changes |

## Testing

- Rotate slowly → verify single-step events
- Rotate quickly → ensure no missed steps or oscillations
- Confirm direction matches UI expectations; swap A/B if needed

## References

- [Rotary Encoder](../ROTARY_ENCODER.md) — firmware integration
- [Hardware Pins](HARDWARE_PINS.md) — GPIO assignments
