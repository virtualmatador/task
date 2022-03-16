cmake_minimum_required(VERSION 3.13)
add_library("task" INTERFACE)
target_include_directories("task" INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries("task" INTERFACE "pthread")
