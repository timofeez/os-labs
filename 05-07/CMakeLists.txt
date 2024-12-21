cmake_minimum_required(VERSION 3.10)
project(lab7_project)

add_executable(lab7 main.cpp src/manager.cpp)
target_include_directories(lab7 PUBLIC include)
target_link_libraries(lab7 zmq)

add_executable(node src/node.cpp)
target_include_directories(node PUBLIC include)
target_link_libraries(node zmq)
