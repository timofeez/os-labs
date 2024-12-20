cmake_minimum_required(VERSION 3.5)
project(lab4_dynamic_libs C)
set(CMAKE_C_STANDARD 99)
include_directories(include)

add_library(lib1 SHARED src/lib1.c)
add_library(lib2 SHARED src/lib2.c)

add_executable(main_static main_static.c)
target_link_libraries(main_static lib1)

add_executable(main_dynamic main_dynamic.c)