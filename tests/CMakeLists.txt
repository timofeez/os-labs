project(mai-os)
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.11.0
)

FetchContent_MakeAvailable(googletest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

add_executable(01_test lab1_test.cpp)
target_include_directories(01_test PUBLIC ../01/include)
target_link_libraries(01_test PRIVATE utils parent gtest_main)

add_library(bitonic_sort_lib
    ../02/src/bitonic_sort.c
    ../02/src/bitonic_data.c
    ../02/src/globals.c
    ../02/include/utils.h
)

target_include_directories(bitonic_sort_lib PUBLIC ../02/include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(bitonic_sort_lib PUBLIC Threads::Threads)

add_executable(02_test lab2_test.cpp)
target_include_directories(02_test PRIVATE ../02/include)
target_link_libraries(02_test PRIVATE bitonic_sort_lib gtest_main)

set_target_properties(02_test PROPERTIES CXX_STANDARD 17)

enable_testing()
add_test(NAME 01_test COMMAND 01_test)
add_test(NAME 02_test COMMAND 02_test)
