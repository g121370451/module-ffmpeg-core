cmake_minimum_required(VERSION 3.27)
project(FFmpegApiTest)

# 1. 确保 C++ 20 开启
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 2. 查找依赖
find_package(spdlog CONFIG REQUIRED)
find_package(GTest REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# 3. 定义可执行文件
add_executable(FFmpegApiTest test_media_processor.cpp)
SET_TARGET_PROPERTIES(FFmpegApiTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/cpp-lib")
# 4. 资源同步逻辑 (改进版)
set(TEST_ASSETS
    "test.gif"
    "test.jpg"
    "test.mp4"
)
set(TEST_DATA_DEST "$<TARGET_FILE_DIR:FFmpegApiTest>")
foreach(ASSET ${TEST_ASSETS})
    add_custom_command(
        TARGET FFmpegApiTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/${ASSET}"
        "${TEST_DATA_DEST}/${ASSET}"
        COMMENT "Copying asset: ${ASSET} to output directory"
    )
endforeach()

# 5. 包含路径 (改进版：确保能搜到核心库的头文件)
target_include_directories(FFmpegApiTest PRIVATE
    .
    "${CMAKE_SOURCE_DIR}/ffmpeg-api-lib" # 确保路径正确指向 MediaProcessor.h 所在位置
)

# 6. 链接库
target_link_libraries(FFmpegApiTest PRIVATE
    FFmpegApiLib
    GTest::gtest_main
    GTest::gtest
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    $<$<PLATFORM_ID:Windows>:ws2_32> # 更通用的 Windows Socket 链接判断
)

# 7. GTest 注册
include(GoogleTest)
# 加上 WORKING_DIRECTORY，确保测试运行时能找到同级目录的文件
gtest_discover_tests(FFmpegApiTest 
    WORKING_DIRECTORY "${TEST_DATA_DEST}"
)