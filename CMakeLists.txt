cmake_minimum_required(VERSION 3.15...3.31)

# 宏定义
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_C_COMPILER "cl")
set(CMAKE_CXX_COMPILER "cl")
set(CMAKE_TOOLCHAIN_FILE "E:/project/github/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET "x64-windows-static")
add_compile_options(/utf-8)

project(module-ffmplayer-core)
add_definitions(-DNAPI_VERSION=8)

file(GLOB SOURCE_FILES
        ./node-api/MediaManagerWrapper.cpp
)
add_library(module-ffmplayer-core SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})

add_subdirectory(ffmpeg-api-lib)
add_subdirectory(ffmpeg-api-test)
set_target_properties(module-ffmplayer-core PROPERTIES PREFIX "" SUFFIX ".node")
target_include_directories(module-ffmplayer-core PRIVATE
        ${CMAKE_JS_INC}
        ./node-api
)

target_link_libraries(module-ffmplayer-core PRIVATE
        FFmpegApiLib
        ${CMAKE_JS_LIB})
target_compile_features(module-ffmplayer-core PRIVATE cxx_std_20)

if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
        # Generate node.lib
        execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()
