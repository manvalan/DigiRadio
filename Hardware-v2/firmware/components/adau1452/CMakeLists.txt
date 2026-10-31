# The SigmaStudio+ exports are optional at build time: see ../../dsp/README.md.
set(DSP_DIR "${CMAKE_CURRENT_LIST_DIR}/../../dsp")

set(ADAU1452_EMBED)
foreach(blob adau1452_program.bin adau1452_selfboot.bin adau1452_eeprom_writer.bin)
    if(EXISTS "${DSP_DIR}/${blob}")
        list(APPEND ADAU1452_EMBED "${DSP_DIR}/${blob}")
    endif()
endforeach()

idf_component_register(
    SRCS
        "src/Adau1452Driver.cpp"
        "src/Adau1452Dsp.cpp"
        "src/Adau1452Images.cpp"
        "src/Adau1452Eeprom.cpp"
        "src/Adau1452Store.cpp"
    INCLUDE_DIRS "include" "${DSP_DIR}"
    REQUIRES core esp_driver_gpio esp_driver_spi freertos
    PRIV_REQUIRES nvs_flash esp_rom esp_partition
    EMBED_FILES ${ADAU1452_EMBED}
)

foreach(blob program selfboot eeprom_writer)
    if(EXISTS "${DSP_DIR}/adau1452_${blob}.bin")
        string(TOUPPER "${blob}" name)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE "ADAU1452_HAS_${name}=1")
    endif()
endforeach()

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
