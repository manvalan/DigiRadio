idf_component_register(
    SRCS
        "src/TunerService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
