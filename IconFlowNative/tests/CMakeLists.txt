# Standalone CMake for ColorMathTests (no AE SDK required)
cmake_minimum_required(VERSION 3.20)
project(ColorMathTests CXX)
set(CMAKE_CXX_STANDARD 17)

add_executable(ColorMathTests ColorMathTests.cpp)
target_include_directories(ColorMathTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")

enable_testing()
add_test(NAME ColorMath COMMAND ColorMathTests)
