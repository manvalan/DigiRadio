# Bring-up library shared by the v2 hardware test and debug monitor.
# Pins come from the v2 firmware, the single source of truth.
idf_component_register(
    SRCS "src/hal.cpp" "src/selftest.cpp" "src/commands.cpp" "src/wireless.cpp"
    INCLUDE_DIRS "include" "${CMAKE_CURRENT_LIST_DIR}/../../../firmware/main"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_uart
             esp_driver_pcnt esp_driver_i2s console esp_timer
             esp_wifi esp_netif esp_event nvs_flash bt
)
