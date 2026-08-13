cmake_minimum_required(VERSION 3.13)

# Bluepad32's supported PlatformIO integration is the upstream ESP-IDF +
# Arduino template, retained as a pinned submodule. It supplies Bluepad32,
# BTstack, its Arduino bridge, and the matching Arduino core component.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/third_party/bluepad32-template/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(technic_rc_car_v2)
