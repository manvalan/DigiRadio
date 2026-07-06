set(ADAU_FW_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../Firmware/ADAU1701-Firmware")

idf_component_register(
    SRCS
        "src/Adau1701Driver.cpp"
        "src/Adau1701Dsp.cpp"
        "src/SigmaStudioFW.c"
        "src/adau1701_program.c"
    INCLUDE_DIRS
        "include"
        "${ADAU_FW_DIR}"
    REQUIRES core driver esp_driver_gpio esp_driver_i2c
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
