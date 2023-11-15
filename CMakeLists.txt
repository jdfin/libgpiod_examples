cmake_minimum_required(VERSION 3.0.0)
project(gpiod_examples VERSION 0.0.1)

add_compile_options(-Wall)

add_executable(output_simple output_simple.cpp)
target_link_libraries(output_simple gpiod)

add_executable(input_simple input_simple.cpp)
target_link_libraries(input_simple gpiod)

add_executable(input_events input_events.cpp)
target_link_libraries(input_events gpiod)
