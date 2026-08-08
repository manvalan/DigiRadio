idf_component_register(
    SRCS
        "main.cpp"
        "hardware_bootstrap.cpp"
        "web_radio_stream.cpp"
        "$<$<BOOL:${CONFIG_TEST_FIRMWARE}>:test_firmware.cpp>"
        "$<$<BOOL:${CONFIG_I2S_SDATA_PROBE}>:i2s_sdata_probe.cpp>"
    INCLUDE_DIRS "."
    REQUIRES core net secure_store adau1701 si4684 tuner audio bt1035 bluetooth station integration ota eeprom24aa webradio driver
    PRIV_REQUIRES esp_timer esp_http_client
)

if(CONFIG_TEST_FIRMWARE)
    set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS}" "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
