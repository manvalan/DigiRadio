# The v2 board (the only esp32s31 target) has an ADAU1452 instead, and IDF 6
# moved Wi-Fi provisioning out of the framework into network_provisioning.
if(IDF_TARGET STREQUAL "esp32s31")
    set(NET_BOARD_REQUIRES "espressif__network_provisioning")
else()
    set(NET_BOARD_REQUIRES "adau1701" "wifi_provisioning")
endif()

idf_component_register(
    SRCS
        "src/SoftApConfig.cpp"
        "src/SoftApHost.cpp"
        "src/StaClient.cpp"
        "src/SetupWebServer.cpp"
        "src/SigmaStudioTcpServer.cpp"
        "src/WifiScanner.cpp"
        "src/NetBootstrap.cpp"
        "src/BleProvisioning.cpp"
    INCLUDE_DIRS "include"
    EMBED_FILES "www/index.html.gz"
    REQUIRES core esp_wifi esp_netif esp_event nvs_flash esp_http_server mdns lwip secure_store tuner audio bluetooth station integration ota bt1035 webradio ${NET_BOARD_REQUIRES}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
