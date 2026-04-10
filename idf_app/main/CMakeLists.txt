idf_component_register(
    SRCS "progress_ui.cpp" "haptic.cpp" "encoder.cpp" "main.cpp"
         "display_driver.cpp" "touch_driver.cpp" "backlight.cpp"
         "wifi_manager.cpp" "wifi_provision.cpp" "wifi_status_ui.cpp"
         "battery.cpp" "battery_ui.cpp" "status_bar.cpp"
         "bridge_discovery.cpp" "connection_ui.cpp"
         "artwork_ui.cpp" "metadata_ui.cpp" "transport_ui.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_timer
             lvgl__lvgl espressif__esp_lcd_sh8601 espressif__esp_lvgl_port
             i2c_bsp lcd_touch_bsp
             esp_wifi esp_netif esp_event esp_http_server dns_server
    PRIV_REQUIRES nvs_flash esp_driver_ledc esp_driver_spi esp_driver_gpio
                  esp_adc espressif__mdns espressif__esp_websocket_client
                  espressif__cjson esp_http_client
)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++20 -fno-exceptions -fno-rtti)
