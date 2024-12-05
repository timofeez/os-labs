cmake_minimum_required(VERSION 3.10)

project(BitonicSort C)

set(CMAKE_C_STANDARD 99)

include_directories(include)

add_executable(bitonic_sort
    main.c
    src/bitonic_data.c
    src/bitonic_sort.c
)
