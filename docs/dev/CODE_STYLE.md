# Code Style — C++20 / ESP-IDF

## Core Rules

1. **C++20 standard** — Use `constexpr`, `concepts`, `std::span`, structured bindings, `[[nodiscard]]`
2. **RAII everywhere** — Resources acquired in constructor, released in destructor
3. **No `new`/`malloc` at runtime** — Pre-allocate at init. Use pools, arenas, static buffers
4. **No exceptions** — `-fno-exceptions`. Use return codes or `std::expected` (C++23) / error enums
5. **No RTTI** — `-fno-rtti`. Use compile-time polymorphism (templates, concepts)
6. **`const` by default** — Mark everything `const` unless mutation needed
7. **`constexpr` when possible** — Compute at compile time

## Naming

```cpp
namespace rs520 // snake_case namespaces
{
class DisplayDriver        // PascalCase classes
{
    void flush_buffer();   // snake_case methods
    int pixel_count_;      // snake_case + trailing underscore for members
    static constexpr int kMaxRetries = 3;  // k-prefix for constants
};
}  // namespace rs520
```

## Memory

```cpp
// Good: static buffer, sized at compile time
static constexpr size_t kBufSize = LCD_W * 36 * sizeof(uint16_t);
alignas(4) static uint8_t draw_buf_[kBufSize];

// Good: RAII wrapper for heap allocation at init
class PsramBuffer
{
public:
    explicit PsramBuffer(size_t size)
        : ptr_(static_cast<uint8_t*>(
              heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM))) {}
    ~PsramBuffer() { free(ptr_); }
    uint8_t* get() { return ptr_; }
private:
    uint8_t* ptr_;
};

// Bad: runtime allocation
auto* buf = new uint8_t[size];  // NO
```

## FreeRTOS Patterns

```cpp
// Task wrapper with RAII
class Task
{
public:
    Task(const char* name, void(*fn)(void*), uint32_t stack, UBaseType_t prio)
    { xTaskCreate(fn, name, stack, this, prio, &handle_); }
    ~Task() { if (handle_) vTaskDelete(handle_); }
private:
    TaskHandle_t handle_ = nullptr;
};

// Mutex guard
class MutexLock
{
public:
    explicit MutexLock(SemaphoreHandle_t m) : mtx_(m) { xSemaphoreTake(mtx_, portMAX_DELAY); }
    ~MutexLock() { xSemaphoreGive(mtx_); }
private:
    SemaphoreHandle_t mtx_;
};
```

## ISR Safety

- Only `FromISR` FreeRTOS variants inside ISR/timer callbacks
- No `ESP_LOG*` from ISR
- No heap allocation from ISR
- Keep ISR handlers minimal — queue event, process in task

## Includes

```cpp
// Order: project headers, ESP-IDF, LVGL, stdlib
#include "display_driver.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include <cstdint>
```

**Never rely on transitive includes.** ESP-IDF v6.0 removed many transitive includes. Include every header for every type you use explicitly (e.g., `driver/gpio.h` for `gpio_num_t`).

## Struct Initialization (ESP-IDF / Third-party)

ESP-IDF v6.0 compiles with `-Werror=missing-field-initializers`. **Do not use C++20 designated initializers** for ESP-IDF or third-party structs — they break when fields are added/reordered across versions.

```cpp
// Good: zero-init + field assignment — safe, order-independent
spi_bus_config_t bus_cfg = {};
bus_cfg.data0_io_num    = GPIO_NUM_15;
bus_cfg.sclk_io_num     = GPIO_NUM_13;
bus_cfg.max_transfer_sz = 360 * 360 * 2;

// Bad: designated initializers — fragile with third-party structs
const spi_bus_config_t bus_cfg = {
    .data0_io_num = GPIO_NUM_15,    // breaks if IDF adds fields
    .sclk_io_num  = GPIO_NUM_13,
};
```

Also avoid C macros from managed components that use C idioms (e.g., bare `-1` for GPIO). Replicate configs manually with proper `static_cast`.

## Error Handling

```cpp
// ESP-IDF style: check every return
esp_err_t ret = some_esp_function();
if (ret != ESP_OK)
{
    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(ret));
    return ret;
}

// Use [[nodiscard]] on functions that return errors
[[nodiscard]] esp_err_t init_display();
```

## Formatting

- 4 spaces indent (no tabs)
- 100 char line limit
- Braces on new line for functions and control flow
