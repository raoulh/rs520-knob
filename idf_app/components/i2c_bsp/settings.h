#ifndef SETTINGS_H
#define SETTINGS_H

#include "driver/gpio.h"
#include "driver/i2c_types.h"

// I2C bus pins (shared: touch + haptic)
#define ESP32_SCL_NUM  (GPIO_NUM_12)
#define ESP32_SDA_NUM  (GPIO_NUM_11)

// CST816 touch controller
#define TOUCH_ADDR     0x15

// DRV2605 haptic motor driver
#define DRV2605_ADDR   0x5A

#endif // SETTINGS_H
