cmake_minimum_required(VERSION 3.15)
project(Rysual LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# find FFmpeg
find_package(PkgConfig REQUIRED)

pkg_check_modules(AVFORMAT  REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC   REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL    REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE   REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
pkg_check_modules(AVFILTER REQUIRED IMPORTED_TARGET libavfilter)

# find SDL3
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)

find_package(Threads REQUIRED)
# 如果是静态链接 SDL3，可能需要同时查找 SDL3_main 并链接 SDL3::SDL3_main
# 同时需要附加
# find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3 SDL3_main)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 基础组件测试 使用 ctest 运行
include(CTest)

add_library(rysual_core STATIC
    src/Log.cpp
    src/media/Resampler.cpp
    src/output/AudioTap.cpp
    src/play/AudioPlayer.cpp
    src/play/AudioPipeline.cpp
    src/visual/FFT.cpp
    src/visual/Visualize.cpp
)

target_include_directories(rysual_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接库
target_link_libraries(rysual_core PUBLIC
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVUTIL
    PkgConfig::SWRESAMPLE
    PkgConfig::SWSCALE
    SDL3::SDL3
    Threads::Threads
    PkgConfig::AVFILTER     # 启用滤镜
    # SDL3::SDL3_main       # 静态链接 SDL3 且需要入口替换时取消注释
)

target_compile_options(rysual_core PRIVATE -Wall -Wextra)

add_executable(Rysual
    main.cpp
)

target_link_libraries(Rysual PRIVATE
    rysual_core
)

target_compile_options(Rysual PRIVATE -Wall -Wextra)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
