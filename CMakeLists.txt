cmake_minimum_required(VERSION 3.20)
project(AegisAntiCheat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "Aegis AntiCheat target platform is Windows only.")
endif()

add_executable(aegis_anticheat
    src/main.cpp
    src/logger.cpp
    src/config.cpp
    src/hwid.cpp
    src/process_monitor.cpp
    src/integrity.cpp
    src/screenshot.cpp
    src/input_monitor.cpp
    src/device_enum.cpp
    src/av_status.cpp
)

target_include_directories(aegis_anticheat PRIVATE include)

# Windows system libs needed across modules
target_link_libraries(aegis_anticheat PRIVATE
    gdi32
    gdiplus
    setupapi
    cfgmgr32
    version
    wbemuuid
    ole32
    oleaut32
    wtsapi32
)

target_compile_definitions(aegis_anticheat PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)
