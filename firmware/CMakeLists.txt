cmake_minimum_required(VERSION 3.13)

# Settings (board, SDK version, toolchain) come from CMakePresets.json.
set(PICO_BOARD pico2_w CACHE STRING "Pico board")

set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/external/bluepad32)

# Must be before project(): prebuilt host tools, then find or download the Pico SDK.
include(cmake/pico_host_tools.cmake)
include(pico_sdk_import.cmake)

# Use the BTstack bundled with the Pico SDK (Bluepad32 example default).
set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)

project(car_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(car_firmware
    src/main.c
    src/controller.c
    src/pad_text.c
    src/drive.c
    src/motor.c
    src/servo.c
)

# btstack_config.h and sdkconfig.h live in src/; Bluepad32 includes them too.
include_directories(src)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
include_directories(${BTSTACK_ROOT}/src)

target_link_libraries(car_firmware PUBLIC
    pico_stdlib
    hardware_pwm
    hardware_watchdog
    pico_cyw43_arch_none
    pico_btstack_classic
    pico_btstack_cyw43
    bluepad32
)

add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)

# printf goes to USB serial (COM port), not the UART pins.
pico_enable_stdio_usb(car_firmware 1)
pico_enable_stdio_uart(car_firmware 0)

# Also make .uf2 (for picotool / drag-and-drop) next to the .elf.
pico_add_extra_outputs(car_firmware)
