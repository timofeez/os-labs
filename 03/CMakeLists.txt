set(CMAKE_C_STANDARD 11)
project(mai-os)

add_library(03_utils src/utils.c)
target_include_directories(03_utils PRIVATE include)

add_library(03_parent src/parent.c)
target_include_directories(03_parent PRIVATE include)
target_link_libraries(03_parent 03_utils)

add_executable(03_child1 src/child1.c)
target_include_directories(03_child1 PRIVATE include)
target_link_libraries(03_child1 03_utils)

add_executable(03_child2 src/child2.c)
target_include_directories(03_child2 PRIVATE include)
target_link_libraries(03_child2 03_utils)

add_executable(lab3 main.c)
target_include_directories(lab3 PRIVATE include)
target_link_libraries(lab3 03_parent)

add_dependencies(lab3 03_child1 03_child2)
