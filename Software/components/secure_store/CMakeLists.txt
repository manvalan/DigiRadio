idf_component_register(
    SRCS "src/NvsSecureStore.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core nvs_flash
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
