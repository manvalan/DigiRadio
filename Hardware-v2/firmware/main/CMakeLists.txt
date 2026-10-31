# Sources without board differences come from the v1 firmware, and so do the
# headers: hardware_bootstrap.hpp and esp32_i2s_sink.hpp are the same interface.
set(V1_MAIN "${CMAKE_CURRENT_LIST_DIR}/../../../Software/main")

idf_component_register(
    SRCS
        "main.cpp"
        "board_v2.cpp"
        "hardware_bootstrap.cpp"
        "esp32_i2s_sink.cpp"
        "power_monitor.cpp"
        "sigma_bridge.cpp"
        "${V1_MAIN}/web_radio_stream.cpp"
        "${V1_MAIN}/web_radio_fetch.cpp"
        "${V1_MAIN}/audio_routing.cpp"
        "${V1_MAIN}/phone_pcm_server.cpp"
        "${V1_MAIN}/system_diag.cpp"
        "${V1_MAIN}/phone_stream.cpp"
        "${V1_MAIN}/antenna_calibration.cpp"
        "${V1_MAIN}/alarm_clock.cpp"
        "${V1_MAIN}/dab_slideshow.cpp"
    INCLUDE_DIRS "." "${V1_MAIN}"
    REQUIRES core net secure_store adau1452 si4684 tuner audio bt1035 bluetooth station integration ota eeprom24aa webradio esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_spi esp_driver_pcnt
    PRIV_REQUIRES esp_timer esp_netif nvs_flash esp_http_client mbedtls app_update lwip mdns esp_partition spi_flash bt esp_app_format
)
