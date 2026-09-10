cmake_minimum_required(VERSION 3.20)

project(personal_stl LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

function(add_stl_test target source)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug"
       AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()

    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_stl_test(circular_queue_test CircularQueue/test.cpp)
add_stl_test(hash_map_test HashMap/test.cpp)
add_stl_test(list_test List/test.cpp)
add_stl_test(tuple_test Tuple/test.cpp)
add_stl_test(variant_test Variant/test.cpp)
add_stl_test(vector_test Vector/test.cpp)

option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)

if(BUILD_BENCHMARKS)
    find_package(benchmark CONFIG REQUIRED)

    add_executable(heap_benchmark Heap/test.cpp)
    target_compile_features(heap_benchmark PRIVATE cxx_std_20)
    target_compile_options(heap_benchmark PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
    )
    target_link_libraries(heap_benchmark PRIVATE benchmark::benchmark_main)
endif()
