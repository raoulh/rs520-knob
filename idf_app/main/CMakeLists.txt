idf_component_register(
    SRCS "progress_ui.cpp" "haptic.cpp" "encoder.cpp" "main.cpp" "display_driver.cpp" "touch_driver.cpp" "backlight.cpp"         "encoder.cpp" "haptic.cpp" "progress_ui.cpp"    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_timer
             lvgl__lvgl espressif__esp_lcd_sh8601 espressif__esp_lvgl_port
             i2c_bsp lcd_touch_bsp
    PRIV_REQUIRES nvs_flash esp_driver_ledc esp_driver_spi esp_driver_gpio
)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++20 -fno-exceptions -fno-rtti)
