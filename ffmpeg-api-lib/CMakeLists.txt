cmake_minimum_required(VERSION 3.27)
message(STATUS "lib generate")
set(CMAKE_CXX_STANDARD 20)
project(FFmpegApiLib)
message(STATUS "PRO_DIR ${PROJECT_SOURCE_DIR}")
message(STATUS "CMAKE_DIR ${CMAKE_SOURCE_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/cpp-lib")
find_package(spdlog CONFIG REQUIRED)
# 1. 引入 PkgConfig 模块
find_package(PkgConfig REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
# 2. 查找 FFmpeg 模块 (REQUIRED 确保找不到就报错)
# IMPORTED_TARGET 会自动创建 PkgConfig::FFMPEG 这样的目标
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
        libavfilter
)

add_library(FFmpegApiLib STATIC
        ./encoder/Encoders.cpp
        ./entity/StreamContext.cpp
        ./manager/MediaManager.cpp
        ./utils/MediaProcessor.cpp
        ./utils/TimerSleep.cpp
        ./manager/SyncClock.cpp
)
target_include_directories(FFmpegApiLib PUBLIC
        ./encoder
        ./entity
        ./manager
        ./utils
)

target_link_libraries(FFmpegApiLib
        PUBLIC
        PkgConfig::FFMPEG
        nlohmann_json::nlohmann_json
        magic_enum::magic_enum
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)