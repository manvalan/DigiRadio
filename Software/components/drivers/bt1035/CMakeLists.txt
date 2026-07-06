idf_component_register(
    SRCS "src/Bt1035Driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core driver esp_driver_gpio esp_driver_uart
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
