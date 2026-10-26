set(SI4684_FW_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../Firmware/Si4684-Firmware")

set(SI4684_SRCS
    "src/Si4684Driver.cpp"
    "src/Si4684ImageStore.cpp"
    "src/Si4684Tuner.cpp")
set(SI4684_EMBED)

# CONFIG_* is not defined during the early requirements scan.
if(CONFIG_SI4684_EMBEDDED_IMAGES)
    if(NOT EXISTS "${SI4684_FW_DIR}/fm_firmware.bin")
        message(FATAL_ERROR
            "Missing ${SI4684_FW_DIR}/fm_firmware.bin — run:\n"
            "  python3 tools/fetch_si4684_firmware.py --from-ugreen-radio-cli <Files_v16.zip>\n"
            "  python3 tools/fetch_si4684_firmware.py --si46xx-dir <si46xx_firmware>\n"
            "or build without them: CONFIG_SI4684_EMBEDDED_IMAGES=n")
    endif()
    list(APPEND SI4684_SRCS "src/Si4684EmbeddedImages.cpp")
    set(SI4684_EMBED
        "${SI4684_FW_DIR}/rom_patch_016.bin"
        "${SI4684_FW_DIR}/dab_firmware.bin"
        "${SI4684_FW_DIR}/fm_firmware.bin")
endif()

idf_component_register(
    SRCS ${SI4684_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES core driver esp_driver_gpio esp_driver_spi esp_partition spi_flash
    EMBED_FILES ${SI4684_EMBED}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
