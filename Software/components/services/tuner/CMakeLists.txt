idf_component_register(
    SRCS
        "src/TunerService.cpp"
        "src/AfFollowService.cpp"
        "src/SpectrumScanService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core freertos
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
