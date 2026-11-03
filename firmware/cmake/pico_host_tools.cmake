# On Windows, use Raspberry Pi's prebuilt picotool instead of letting the SDK
# build it. SDK 2.1.1 builds picotool from its unpinned "develop" branch; that
# build crashed here (picotool coprodis, access violation).
# Downloaded once into .deps/ and checked by SHA-256.
#
# pioasm is still built by the SDK (needs CXXFLAGS=-include cstdint, set in
# CMakePresets.json). The prebuilt pioasm.exe from pico-sdk-tools was
# quarantined by Windows Defender on this laptop (likely false positive).

if(NOT WIN32 OR DEFINED picotool_DIR)
    return()
endif()

set(_tools_release v2.1.1-3)
set(_tools_dir ${CMAKE_CURRENT_LIST_DIR}/../.deps/tools-2.1.1)
set(_tools_url https://github.com/raspberrypi/pico-sdk-tools/releases/download/${_tools_release})

function(_fetch_pico_tool zip_name sha256)
    set(zip ${_tools_dir}/${zip_name}.zip)
    set(out ${_tools_dir}/${zip_name})
    if(NOT EXISTS ${out})
        message(STATUS "Downloading ${zip_name}")
        # Windows' own curl.exe uses the Windows certificate store. (CMake's
        # file(DOWNLOAD) in MinGW builds has no CA bundle: TLS error 60.)
        file(MAKE_DIRECTORY ${_tools_dir})
        execute_process(
            COMMAND $ENV{SystemRoot}/System32/curl.exe -fsSL -o ${zip} ${_tools_url}/${zip_name}.zip
            RESULT_VARIABLE rc)
        if(NOT rc EQUAL 0)
            message(FATAL_ERROR "Download of ${zip_name} failed (curl exit ${rc})")
        endif()
        file(SHA256 ${zip} actual)
        if(NOT actual STREQUAL sha256)
            file(REMOVE ${zip})
            message(FATAL_ERROR "${zip_name}.zip: SHA-256 mismatch (got ${actual})")
        endif()
        file(ARCHIVE_EXTRACT INPUT ${zip} DESTINATION ${out})
    endif()
endfunction()

_fetch_pico_tool(picotool-2.1.1-x64-win
    b76fc05656ef849e16bb19aa4bbb9eb92cbc18df7fe72782e988a3dfc33730c2)

get_filename_component(_tools_dir ${_tools_dir} ABSOLUTE)
set(picotool_DIR ${_tools_dir}/picotool-2.1.1-x64-win/picotool CACHE PATH "Prebuilt picotool")
