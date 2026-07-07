idf_component_register(
    SRCS
        "src/OtaService.cpp"
        "src/OtaError.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core app_update esp_partition
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
