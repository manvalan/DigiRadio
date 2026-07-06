# DigiRadio firmware — top-level ESP-IDF project
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Nested driver components live under components/drivers/
set(EXTRA_COMPONENT_DIRS "components/drivers")

project(digiradio)
