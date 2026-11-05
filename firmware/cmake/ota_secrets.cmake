# Reads firmware/.env (KEY=VALUE lines, git-ignored) and writes
# <build>/generated/ota_secrets.h with WIFI_SSID, WIFI_PASSWORD, OTA_TOKEN.
# tools/ota.mjs reads the same file, so the token only lives in one place.
#
# No .env -> configure stops. An image without Wi-Fi settings, once flashed
# over the air, could never be updated over the air again.

set(OTA_ENV_FILE ${CMAKE_CURRENT_SOURCE_DIR}/.env CACHE FILEPATH "Wi-Fi/OTA settings file")

if(NOT EXISTS ${OTA_ENV_FILE})
    message(FATAL_ERROR
        "Missing ${OTA_ENV_FILE}\n"
        "Copy firmware/.env.example to firmware/.env and fill in your Wi-Fi and OTA token.")
endif()

# Re-run configure when .env changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${OTA_ENV_FILE})

file(STRINGS ${OTA_ENV_FILE} _env_lines ENCODING UTF-8)
foreach(_line IN LISTS _env_lines)
    if(_line MATCHES "^[ \t]*([A-Z_]+)[ \t]*=(.*)$")
        set(_key ${CMAKE_MATCH_1})
        string(STRIP "${CMAKE_MATCH_2}" _value)
        set(_ENV_${_key} "${_value}")
    endif()
endforeach()

foreach(_key WIFI_SSID WIFI_PASSWORD OTA_TOKEN)
    if(NOT DEFINED _ENV_${_key} OR "${_ENV_${_key}}" STREQUAL "")
        message(FATAL_ERROR "${OTA_ENV_FILE}: ${_key} is missing or empty")
    endif()
endforeach()

string(LENGTH "${_ENV_OTA_TOKEN}" _token_len)
if(_token_len GREATER 47)
    message(FATAL_ERROR "${OTA_ENV_FILE}: OTA_TOKEN is longer than 47 characters")
endif()

# Make a C string literal: escape backslash and double quote.
function(_ota_c_string out value)
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    set(${out} "\"${value}\"" PARENT_SCOPE)
endfunction()

_ota_c_string(OTA_C_WIFI_SSID "${_ENV_WIFI_SSID}")
_ota_c_string(OTA_C_WIFI_PASSWORD "${_ENV_WIFI_PASSWORD}")
_ota_c_string(OTA_C_TOKEN "${_ENV_OTA_TOKEN}")

set(OTA_SECRETS_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(CONFIGURE OUTPUT ${OTA_SECRETS_DIR}/ota_secrets.h CONTENT
"// Generated from ${OTA_ENV_FILE} by cmake/ota_secrets.cmake. Do not commit.
#pragma once
#define WIFI_SSID ${OTA_C_WIFI_SSID}
#define WIFI_PASSWORD ${OTA_C_WIFI_PASSWORD}
#define OTA_TOKEN ${OTA_C_TOKEN}
")
message(STATUS "Wi-Fi: \"${_ENV_WIFI_SSID}\" (from ${OTA_ENV_FILE})")
