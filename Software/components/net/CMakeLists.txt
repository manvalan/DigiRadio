idf_component_register(
    SRCS
        "src/SoftApConfig.cpp"
        "src/SoftApHost.cpp"
        "src/StaClient.cpp"
        "src/SetupWebServer.cpp"
        "src/NetBootstrap.cpp"
    INCLUDE_DIRS "include"
    EMBED_FILES "www/index.html.gz"
    REQUIRES core esp_wifi esp_netif esp_event nvs_flash esp_http_server mdns secure_store tuner audio bluetooth station integration bt1035 adau1701
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
