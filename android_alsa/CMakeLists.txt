cmake_minimum_required(VERSION 2.8)

project(AndroidAlsa C)
message("Building ${PROJECT_NAME}")

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O2 -fPIC -DPIC")

MESSAGE(STATUS "Compiler options: ${CMAKE_C_FLAGS}")

include_directories(include)

add_library(asound_module_pcm_android_aserver SHARED module_pcm_android_aserver.c)
target_link_libraries(asound_module_pcm_android_aserver "/data/data/com.winlator/files/rootfs/lib/libasound.so.2")