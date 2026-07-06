idf_component_register(
    SRCS
        "src/StationService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core tuner
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
