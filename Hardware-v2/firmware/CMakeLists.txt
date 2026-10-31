# DigiRadio v2 firmware — ESP-IDF project for the batch 2 board.
# Build with ESP-IDF 6.1 (ESP32-S31, preview target). Drivers, services and
# web server are shared with the v1 firmware in ../../Software.
cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(DIGIRADIO_V1 "${CMAKE_CURRENT_LIST_DIR}/../../Software")
set(EXTRA_COMPONENT_DIRS
    "${DIGIRADIO_V1}/components"
    "${DIGIRADIO_V1}/components/drivers"
    "${DIGIRADIO_V1}/components/services")
# adau1701 lives next to the shared drivers; the board has no such chip.
set(EXCLUDE_COMPONENTS adau1701)
# Only what main needs, not every IDF component.
set(COMPONENTS main)

project(digiradio_v2)
