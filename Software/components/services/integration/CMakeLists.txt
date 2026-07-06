idf_component_register(
    SRCS
        "src/IntegrationService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core tuner audio station
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
