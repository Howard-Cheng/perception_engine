cmake_minimum_required(VERSION 3.14)

project(pe_base_test)

add_executable(pe_base_test main.cpp)

target_link_libraries(pe_base_test PRIVATE pe_base)
