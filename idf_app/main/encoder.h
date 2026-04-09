#ifndef RS520_ENCODER_H
#define RS520_ENCODER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <cstdint>

namespace rs520
{

/// Encoder rotation direction
enum class EncoderDir : int8_t
{
    kCW  =  1,  // Clockwise
    kCCW = -1,  // Counter-clockwise
};

/// Initialize rotary encoder polling (GPIO 7/8, 3ms timer).
/// Creates internal FreeRTOS queue for rotation events.
[[nodiscard]] esp_err_t encoder_init();

/// Get queue handle for receiving EncoderDir events.
/// Use xQueueReceive() from consumer task.
QueueHandle_t encoder_queue();

}  // namespace rs520

#endif // RS520_ENCODER_H
