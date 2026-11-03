# PC tests for code that does not need the Pico (built with the host gcc).
cmake_minimum_required(VERSION 3.13)
project(car_host_tests C)
set(CMAKE_C_STANDARD 11)

enable_testing()

add_executable(test_pad_text test_pad_text.c ../src/pad_text.c)
target_include_directories(test_pad_text PRIVATE ../src)
target_compile_options(test_pad_text PRIVATE -Wall -Wextra -Werror)
add_test(NAME pad_text COMMAND test_pad_text)
