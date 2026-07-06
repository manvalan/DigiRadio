# DigiRadio firmware — top-level ESP-IDF project
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Nested driver/service components live under components/
set(EXTRA_COMPONENT_DIRS "components/drivers" "components/services")

project(digiradio)
