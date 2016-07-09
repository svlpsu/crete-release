cmake_minimum_required(VERSION 2.8.7)

project(guest)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}/lib/include")
link_directories(${CMAKE_BINARY_DIR}/bin)

add_subdirectory(lib)
add_subdirectory(util)

