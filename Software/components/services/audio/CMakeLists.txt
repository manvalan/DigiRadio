idf_component_register(
    SRCS
        "src/AudioService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
