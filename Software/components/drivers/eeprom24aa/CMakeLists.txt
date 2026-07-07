idf_component_register(
    SRCS "src/Eeprom24aa.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core driver esp_driver_i2c
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
