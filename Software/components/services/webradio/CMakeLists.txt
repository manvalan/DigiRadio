idf_component_register(
    SRCS
        "src/WebRadioService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core freertos
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
