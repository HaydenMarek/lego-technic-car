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

# Wi-Fi name/password and OTA token from firmware/.env (git-ignored).
include(cmake/ota_secrets.cmake)

add_executable(car_firmware
    src/main.c
    src/controller.c
    src/pad_text.c
    src/drive.c
    src/assist.c
    src/gyro.c
    src/motor.c
    src/servo.c
    src/ota.c
    src/ota_logic.c
    src/flash_bank_fix.c
)

# btstack_config.h, sdkconfig.h and lwipopts.h live in src/; the libraries include them too.
include_directories(src)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
include_directories(${BTSTACK_ROOT}/src)
target_include_directories(car_firmware PRIVATE ${OTA_SECRETS_DIR})

target_compile_definitions(car_firmware PRIVATE
    # Name on the network: DHCP host name and mDNS (lego-car.local).
    CYW43_HOST_NAME=\"lego-car\"
    # "Try before you buy": a new image must call rom_explicit_buy() (ota.c),
    # or the boot ROM goes back to the old one on the next reboot.
    PICO_CRT0_IMAGE_TYPE_TBYB=1
)

target_link_libraries(car_firmware PUBLIC
    pico_stdlib
    pico_flash
    hardware_flash
    hardware_pwm
    hardware_i2c
    hardware_watchdog
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mdns
    pico_btstack_classic
    pico_btstack_cyw43
    bluepad32
)

# Read Bluetooth pairing keys from the physical flash address, also when we
# run from partition B. See src/flash_bank_fix.c.
target_link_options(car_firmware PRIVATE -Wl,--wrap=pico_flash_bank_instance)

add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 libbluepad32)

# Bluepad32 links pico_cyw43_arch_none (CYW43_LWIP=0). We need Wi-Fi, so all
# code must use the lwIP variant: CYW43_LWIP changes the cyw43_t struct
# layout, and a mix would corrupt memory. Swap it without patching the submodule.
foreach(_prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(_libs bluepad32 ${_prop})
    list(TRANSFORM _libs REPLACE "^pico_cyw43_arch_none$" pico_cyw43_arch_lwip_threadsafe_background)
    set_target_properties(bluepad32 PROPERTIES ${_prop} "${_libs}")
endforeach()

# printf goes to USB serial (COM port), not the UART pins.
pico_enable_stdio_usb(car_firmware 1)
pico_enable_stdio_uart(car_firmware 0)

# SHA-256 of the image in its header: the boot ROM refuses a corrupt image
# (e.g. a half-written OTA slot) and boots the other slot.
pico_hash_binary(car_firmware)

# Also make .uf2 (for picotool / drag-and-drop / OTA) next to the .elf.
pico_add_extra_outputs(car_firmware)

# Partition table (two A/B slots) as a UF2. Flash once over USB.
# See firmware/README.md "Over-the-air update".
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/partition_table.uf2
    COMMAND picotool partition create
            ${CMAKE_CURRENT_SOURCE_DIR}/partition_table.json
            ${CMAKE_CURRENT_BINARY_DIR}/partition_table.uf2
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/partition_table.json
    COMMENT "Partition table UF2"
    VERBATIM)
add_custom_target(partition_table ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/partition_table.uf2)
