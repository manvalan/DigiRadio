idf_component_register(
    SRCS
        "src/BluetoothService.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core bt1035
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
