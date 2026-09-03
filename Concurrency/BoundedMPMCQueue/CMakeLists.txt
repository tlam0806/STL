cmake_minimum_required(VERSION 3.20)

project(mpmc_queue LANGUAGES CXX)

find_package(Threads REQUIRED)

add_executable(mpmc_test test.cpp)

target_compile_features(mpmc_test PRIVATE cxx_std_20)

target_compile_options(mpmc_test PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
)

target_link_libraries(mpmc_test PRIVATE Threads::Threads)
