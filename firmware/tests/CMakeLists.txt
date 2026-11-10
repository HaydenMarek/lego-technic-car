# PC tests for code that does not need the Pico (built with the host gcc).
cmake_minimum_required(VERSION 3.13)
project(car_host_tests C)
set(CMAKE_C_STANDARD 11)

enable_testing()

add_executable(test_pad_text test_pad_text.c ../src/pad_text.c)
target_include_directories(test_pad_text PRIVATE ../src)
target_compile_options(test_pad_text PRIVATE -Wall -Wextra -Werror)
add_test(NAME pad_text COMMAND test_pad_text)

add_executable(test_drive test_drive.c ../src/drive.c)
target_include_directories(test_drive PRIVATE ../src)
target_compile_options(test_drive PRIVATE -Wall -Wextra -Werror)
add_test(NAME drive COMMAND test_drive)

add_executable(test_ota_logic test_ota_logic.c ../src/ota_logic.c)
target_include_directories(test_ota_logic PRIVATE ../src)
target_compile_options(test_ota_logic PRIVATE -Wall -Wextra -Werror)
add_test(NAME ota_logic COMMAND test_ota_logic)

add_executable(test_assist test_assist.c ../src/assist.c)
target_include_directories(test_assist PRIVATE ../src)
target_compile_options(test_assist PRIVATE -Wall -Wextra -Werror)
add_test(NAME assist COMMAND test_assist)
