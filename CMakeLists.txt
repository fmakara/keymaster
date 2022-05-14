cmake_minimum_required(VERSION 3.13)
include(pico-sdk/pico_sdk_init.cmake)

project(keymaster)

pico_sdk_init()

add_executable(keymaster
    src/main.cpp
    src/usb/usb_descriptors.cpp
    src/usb/unused_callbacks.cpp
    src/display/ssd1306.cpp
    src/sprite/dict8.cpp
    src/sprite/sprite.cpp
    src/sprite/menu.cpp
    src/pers/persistency.cpp
)

target_include_directories(keymaster PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(keymaster
    pico_stdlib
    hardware_i2c
    hardware_flash
    hardware_adc
    pico_multicore
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(keymaster)