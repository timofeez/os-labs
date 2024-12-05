set(CMAKE_C_STANDARD 11)
project(os-labs)

add_library(utils include/utils.h src/utils.c)
target_include_directories(utils PRIVATE include)

add_library(parent include/parent.h src/parent.c)
target_include_directories(parent PRIVATE include)
target_link_libraries(parent utils)

add_executable(child1 src/child1.c)
target_include_directories(child1 PRIVATE include)
target_link_libraries(child1 utils)

add_executable(child2 src/child2.c)
target_include_directories(child2 PRIVATE include)
target_link_libraries(child2 utils)

add_executable(lab1 main.c)
target_include_directories(lab1 PRIVATE include)
target_link_libraries(lab1 parent)

add_dependencies(lab1 child1 child2)