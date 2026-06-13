cmake_minimum_required(VERSION 3.16)
project(order_book_simulator CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(include src)

# Core library shared by all executables.
add_library(obs_core STATIC
    src/order_book.cpp
    src/data_gen.cpp
)
target_include_directories(obs_core PUBLIC include src)

# CLI: ./obs demo | sim
add_executable(obs src/main.cpp)
target_link_libraries(obs PRIVATE obs_core)

# Benchmark: ./bench [events]
add_executable(bench bench/benchmark.cpp)
target_link_libraries(bench PRIVATE obs_core)

# Tests
enable_testing()
add_executable(tests tests/test_order_book.cpp)
target_link_libraries(tests PRIVATE obs_core)
add_test(NAME order_book_tests COMMAND tests)
