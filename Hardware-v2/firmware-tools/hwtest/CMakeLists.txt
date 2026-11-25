# DigiRadio v2 bring-up: hwtest. ESP-IDF 6.1, target esp32s31 (preview).
cmake_minimum_required(VERSION 3.22)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(digiradio_v2_hwtest)
