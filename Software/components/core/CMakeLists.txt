idf_component_register(
    SRCS
        "src/FirmwareVersion.cpp"
        "src/HealthStatus.cpp"
        "src/HealthStatusJson.cpp"
        "src/Secret.cpp"
        "src/WifiSsid.cpp"
        "src/WifiCredentials.cpp"
        "src/WifiProvisionJson.cpp"
        "src/EmbeddedBlobReader.cpp"
        "src/TunerJson.cpp"
        "src/FrequencyKHz.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
