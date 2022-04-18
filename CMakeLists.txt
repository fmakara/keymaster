cmake_minimum_required(VERSION 3.13)
include(pico-sdk/pico_sdk_init.cmake)

project(keymaster)

pico_sdk_init()

add_executable(keymaster
    src/main.cpp
    src/usb_descriptors.cpp
)

target_include_directories(keymaster PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(keymaster
    pico_stdlib
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(keymaster)