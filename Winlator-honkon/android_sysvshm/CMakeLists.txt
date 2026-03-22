cmake_minimum_required(VERSION 2.8)

project(AndroidSysVSHM C)
message("Building ${PROJECT_NAME}")

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=gnu99 -shared -fPIC")

MESSAGE(STATUS "Compiler options: ${CMAKE_C_FLAGS}")

add_library(android-sysvshm SHARED android_sysvshm.c)