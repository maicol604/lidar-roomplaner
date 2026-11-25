cmake_minimum_required(VERSION 3.22.1)
project(lidarcore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_SOURCES
    core/ld06.cpp
    core/protocol.cpp
    core/clock_sync.cpp
    core/pose_buffer.cpp
    core/config.cpp
    core/coverage_grid.cpp
    core/mapping_engine.cpp
    core/layout_engine.cpp
    mock/apartment.cpp
    mock/tour.cpp
    mock/mock_world.cpp
    io/recorder.cpp
    io/mcap_impl.cpp
    io/exporter.cpp
    io/ply_reader.cpp
    net/udp_transport.cpp
)

add_library(lidarcore_static STATIC ${CORE_SOURCES})
target_include_directories(lidarcore_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} third_party/mcap/include)
# Compression codecs are not bundled; sessions are written uncompressed (chunked + indexed).
target_compile_definitions(lidarcore_static PUBLIC MCAP_COMPRESSION_NO_LZ4 MCAP_COMPRESSION_NO_ZSTD)
target_compile_options(lidarcore_static PRIVATE -Wall -O2)
# The vendored MCAP writer narrows uint64_t sizes into size_t braces, an error on 32-bit ABIs
# (armeabi-v7a). Message sizes are far below 4 GB, so the conversion is safe.
set_source_files_properties(io/mcap_impl.cpp PROPERTIES COMPILE_OPTIONS "-Wno-c++11-narrowing")

add_library(lidarcore SHARED jni_bridge.cpp scanner_core.cpp render/renderer.cpp)
target_compile_options(lidarcore PRIVATE -Wall -O2)
target_link_libraries(lidarcore lidarcore_static android log EGL GLESv3)

option(LIDARCORE_TESTS "Build native unit tests (run on device via adb)" OFF)
if(LIDARCORE_TESTS)
  add_executable(lidarcore_tests tests/core_tests.cpp)
  target_link_libraries(lidarcore_tests lidarcore_static log)
endif()
