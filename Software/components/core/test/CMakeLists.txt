# Host unit tests for components/core (plain CMake + ctest)

cmake_minimum_required(VERSION 3.16)
project(digiradio_core_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

enable_testing()

set(CORE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_library(digiradio_core STATIC
    "${CORE_SRC_DIR}/FirmwareVersion.cpp"
    "${CORE_SRC_DIR}/HealthStatus.cpp"
    "${CORE_SRC_DIR}/HealthStatusJson.cpp"
    "${CORE_SRC_DIR}/Secret.cpp"
    "${CORE_SRC_DIR}/WifiSsid.cpp"
    "${CORE_SRC_DIR}/WifiCredentials.cpp"
    "${CORE_SRC_DIR}/WifiProvisionJson.cpp"
)
target_include_directories(digiradio_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)

add_executable(health_status_test health_status_test.cpp)
target_link_libraries(health_status_test PRIVATE digiradio_core)
add_test(NAME health_status_test COMMAND health_status_test)

add_executable(wifi_provision_test wifi_provision_test.cpp)
target_link_libraries(wifi_provision_test PRIVATE digiradio_core)
add_test(NAME wifi_provision_test COMMAND wifi_provision_test)
