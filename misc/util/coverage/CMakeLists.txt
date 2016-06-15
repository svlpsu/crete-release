cmake_minimum_required(VERSION 2.8.7)

LIST(APPEND CMAKE_CXX_FLAGS -std=c++11)

project(coverage)

add_executable(crete-coverage coverage.cpp)

target_link_libraries(crete-coverage crete_test_case boost_system boost_filesystem boost_serialization boost_program_options)
