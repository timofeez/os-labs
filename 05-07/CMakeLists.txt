set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
cmake_minimum_required(VERSION 3.21)
project(lab57)

add_definitions(-lzmq)

add_compile_options(-pthread)

add_executable(client client.cpp tree.cpp)
add_executable(server main.cpp tree.cpp)

target_link_libraries(client zmq)
target_link_libraries(server zmq)