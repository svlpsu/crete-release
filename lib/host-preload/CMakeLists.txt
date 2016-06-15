cmake_minimum_required(VERSION 2.8.7)

project(host-preload)

#add_definitions(-D_GNU_SOURCE)

add_library(crete_host_preload SHARED preload.cpp)
target_link_libraries(crete_host_preload boost_system boost_filesystem boost_serialization dl)
