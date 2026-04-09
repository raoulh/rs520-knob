#include "haptic.h"

#include "i2c_bsp.h"
#include "esp_log.h"

#include <cstdint>

namespace
{

constexpr const char* kTag = "haptic";

// DRV2605 register addresses
constexpr uint8_t kRegStatus     = 0x00;
constexpr uint8_t kRegMode       = 0x01;
constexpr uint8_t kRegLibrary    = 0x03;
constexpr uint8_t kRegWaveSeq0   = 0x04;
constexpr uint8_t kRegWaveSeq1   = 0x05;
constexpr uint8_t kRegGo         = 0x0C;
constexpr uint8_t kRegOverdrive  = 0x0D;
constexpr uint8_t kRegSustainPos = 0x0E;
constexpr uint8_t kRegSustainNeg = 0x0F;
constexpr uint8_t kRegBreak      = 0x10;
constexpr uint8_t kRegFeedback   = 0x1A;
constexpr uint8_t kRegControl3   = 0x1D;

// Effect IDs (ERM library)
constexpr uint8_t kEffectStrongClick = 1;

static void drv_write(uint8_t reg, uint8_t val)
{
    i2c_write_buff(drv2605_dev_handle, reg, &val, 1);
}

static uint8_t drv_read(uint8_t reg)
{
    uint8_t val = 0;
    i2c_read_buff(drv2605_dev_handle, reg, &val, 1);
    return val;
}

}  // namespace

namespace rs520
{

esp_err_t haptic_init()
{
    ESP_LOGI(kTag, "Initializing DRV2605 haptic driver");

    // Read status to verify device present
    uint8_t status = drv_read(kRegStatus);
    ESP_LOGI(kTag, "DRV2605 status: 0x%02X", status);

    // Mode: internal trigger (default after reset)
    drv_write(kRegMode, 0x00);

    // Select ERM library (library 1)
    drv_write(kRegLibrary, 0x01);

    // Set ERM mode in feedback control register (bit 7 = 0 for ERM)
    uint8_t feedback = drv_read(kRegFeedback);
    feedback &= 0x7F;  // Clear bit 7 → ERM
    drv_write(kRegFeedback, feedback);

    // Overdrive/sustain/brake = 0 for crisp clicks
    drv_write(kRegOverdrive, 0);
    drv_write(kRegSustainPos, 0);
    drv_write(kRegSustainNeg, 0);
    drv_write(kRegBreak, 0);

    ESP_LOGI(kTag, "DRV2605 ready (ERM, internal trigger)");
    return ESP_OK;
}

void haptic_click()
{
    // Skip if previous effect still playing
    uint8_t go = drv_read(kRegGo);
    if (go & 0x01)
    {
        return;
    }

    // Program waveform: Strong Click, then end
    drv_write(kRegWaveSeq0, kEffectStrongClick);
    drv_write(kRegWaveSeq1, 0x00);  // End marker

    // Fire
    drv_write(kRegGo, 0x01);
}

}  // namespace rs520
