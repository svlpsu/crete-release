cmake_minimum_required(VERSION 2.8.7)

set(CMAKE_CXX_COMPILER clang++)

project(crete)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}/lib/include")

add_subdirectory(lib)
add_subdirectory(back-end)
add_subdirectory(front-end)
add_subdirectory(test)
add_subdirectory(misc)

