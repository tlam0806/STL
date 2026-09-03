cmake_minimum_required(VERSION 3.20)

project(personal_concurrency LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()
find_package(Threads REQUIRED)

function(add_concurrency_test target source)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
    )
    target_link_libraries(${target} PRIVATE Threads::Threads)

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

add_concurrency_test(mpmc_queue_test MPMCQueue/test.cpp)
add_concurrency_test(bounded_mpmc_queue_test BoundedMPMCQueue/test.cpp)
add_concurrency_test(ring_buffer_test RingBuffer/test.cpp)
