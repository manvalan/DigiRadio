set(SI4684_FW_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../Firmware/Si4684-Firmware")

if(NOT EXISTS "${SI4684_FW_DIR}/fm_firmware.bin")
    message(FATAL_ERROR
        "Missing ${SI4684_FW_DIR}/fm_firmware.bin — run:\n"
        "  python3 tools/fetch_si4684_firmware.py --from-ugreen-radio-cli <Files_v16.zip>\n"
        "  python3 tools/fetch_si4684_firmware.py --si46xx-dir <si46xx_firmware>")
endif()

idf_component_register(
    SRCS
        "src/Si4684Driver.cpp"
        "src/Si4684EmbeddedImages.cpp"
        "src/Si4684Tuner.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core driver esp_driver_gpio esp_driver_spi
    EMBED_FILES
        "${SI4684_FW_DIR}/rom_patch_016.bin"
        "${SI4684_FW_DIR}/dab_firmware.bin"
        "${SI4684_FW_DIR}/fm_firmware.bin"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
