idf_component_register(
    SRCS
        "main.cpp"
        "hardware_bootstrap.cpp"
    INCLUDE_DIRS "."
    REQUIRES core net secure_store adau1701 si4684 tuner audio bt1035 bluetooth station
)
