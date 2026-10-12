# Host build: core library + unit tests. Firmware lives in firmware/node and
# firmware/gateway and is built with ESP-IDF (idf.py).
cmake_minimum_required(VERSION 3.20)
project(sensor_network_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SN_SANITIZE "Build host tests with ASan and UBSan" ON)
if(SN_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(components/sensor_core)

include(FetchContent)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(sensor_core_tests
    tests/test_hcsr04.cpp
    tests/test_pir.cpp
    tests/test_report.cpp
    tests/test_node_controller.cpp
    tests/test_gateway_tracker.cpp
)
target_link_libraries(sensor_core_tests PRIVATE sensor_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(sensor_core_tests)
