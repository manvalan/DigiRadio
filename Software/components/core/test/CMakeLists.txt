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
    "${CORE_SRC_DIR}/Eui48.cpp"
    "${CORE_SRC_DIR}/DeviceIdentity.cpp"
    "${CORE_SRC_DIR}/Secret.cpp"
    "${CORE_SRC_DIR}/WifiSsid.cpp"
    "${CORE_SRC_DIR}/WifiCredentials.cpp"
    "${CORE_SRC_DIR}/WifiProvisionJson.cpp"
    "${CORE_SRC_DIR}/EmbeddedBlobReader.cpp"
    "${CORE_SRC_DIR}/TunerJson.cpp"
    "${CORE_SRC_DIR}/FrequencyKHz.cpp"
    "${CORE_SRC_DIR}/GainDb.cpp"
    "${CORE_SRC_DIR}/FrequencyHz.cpp"
    "${CORE_SRC_DIR}/EqBandIndex.cpp"
    "${CORE_SRC_DIR}/BiquadCoefficients.cpp"
    "${CORE_SRC_DIR}/BiquadDesign.cpp"
    "${CORE_SRC_DIR}/MixerState.cpp"
    "${CORE_SRC_DIR}/EqProfile.cpp"
    "${CORE_SRC_DIR}/EnhanceLevel.cpp"
    "${CORE_SRC_DIR}/AudioEnhancements.cpp"
    "${CORE_SRC_DIR}/EnhancementsDesign.cpp"
    "${CORE_SRC_DIR}/AudioProfile.cpp"
    "${CORE_SRC_DIR}/AudioProfileJson.cpp"
    "${CORE_SRC_DIR}/Bt1035At.cpp"
    "${CORE_SRC_DIR}/StationName.cpp"
    "${CORE_SRC_DIR}/PresetSlot.cpp"
    "${CORE_SRC_DIR}/Station.cpp"
    "${CORE_SRC_DIR}/StationList.cpp"
    "${CORE_SRC_DIR}/StationListJson.cpp"
    "${CORE_SRC_DIR}/BluetoothJson.cpp"
    "${CORE_SRC_DIR}/BroadcastLabel.cpp"
    "${CORE_SRC_DIR}/RdsMetadataAccumulator.cpp"
    "${CORE_SRC_DIR}/DabDynamicLabelAccumulator.cpp"
)
target_include_directories(digiradio_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)

add_executable(device_identity_test device_identity_test.cpp)
target_link_libraries(device_identity_test PRIVATE digiradio_core)
add_test(NAME device_identity_test COMMAND device_identity_test)

add_executable(health_status_test health_status_test.cpp)
target_link_libraries(health_status_test PRIVATE digiradio_core)
add_test(NAME health_status_test COMMAND health_status_test)

add_executable(wifi_provision_test wifi_provision_test.cpp)
target_link_libraries(wifi_provision_test PRIVATE digiradio_core)
add_test(NAME wifi_provision_test COMMAND wifi_provision_test)

add_executable(embedded_blob_reader_test embedded_blob_reader_test.cpp)
target_link_libraries(embedded_blob_reader_test PRIVATE digiradio_core)
add_test(NAME embedded_blob_reader_test COMMAND embedded_blob_reader_test)

add_executable(tuner_json_test tuner_json_test.cpp)
target_link_libraries(tuner_json_test PRIVATE digiradio_core)
add_test(NAME tuner_json_test COMMAND tuner_json_test)

add_executable(frequency_khz_test frequency_khz_test.cpp)
target_link_libraries(frequency_khz_test PRIVATE digiradio_core)
add_test(NAME frequency_khz_test COMMAND frequency_khz_test)

add_executable(biquad_design_test biquad_design_test.cpp)
target_link_libraries(biquad_design_test PRIVATE digiradio_core)
add_test(NAME biquad_design_test COMMAND biquad_design_test)

add_executable(audio_profile_json_test audio_profile_json_test.cpp)
target_link_libraries(audio_profile_json_test PRIVATE digiradio_core)
add_test(NAME audio_profile_json_test COMMAND audio_profile_json_test)

add_executable(enhancements_design_test enhancements_design_test.cpp)
target_link_libraries(enhancements_design_test PRIVATE digiradio_core)
add_test(NAME enhancements_design_test COMMAND enhancements_design_test)

add_executable(station_service_test station_service_test.cpp)
target_include_directories(station_service_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/station/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/tuner/include"
)
target_sources(station_service_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/station/src/StationService.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/tuner/src/TunerService.cpp"
)
target_link_libraries(station_service_test PRIVATE digiradio_core)
add_test(NAME station_service_test COMMAND station_service_test)

add_executable(station_list_test station_list_test.cpp)
target_link_libraries(station_list_test PRIVATE digiradio_core)
add_test(NAME station_list_test COMMAND station_list_test)

add_executable(bt1035_at_test bt1035_at_test.cpp)
target_link_libraries(bt1035_at_test PRIVATE digiradio_core)
add_test(NAME bt1035_at_test COMMAND bt1035_at_test)

add_executable(broadcast_metadata_test broadcast_metadata_test.cpp)
target_link_libraries(broadcast_metadata_test PRIVATE digiradio_core)
add_test(NAME broadcast_metadata_test COMMAND broadcast_metadata_test)

add_executable(integration_service_test integration_service_test.cpp)
target_include_directories(integration_service_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/integration/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/station/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/tuner/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/audio/include"
)
target_sources(integration_service_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/integration/src/IntegrationService.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/station/src/StationService.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/tuner/src/TunerService.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../services/audio/src/AudioService.cpp"
)
target_link_libraries(integration_service_test PRIVATE digiradio_core)
add_test(NAME integration_service_test COMMAND integration_service_test)
