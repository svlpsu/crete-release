cmake_minimum_required(VERSION 2.8.7)

project(host-harness)

#add_definitions(-D_GNU_SOURCE)

add_library(crete_host_harness SHARED host_harness.cpp)
target_link_libraries(crete_host_harness crete_hook crete_test_case boost_serialization dl)
