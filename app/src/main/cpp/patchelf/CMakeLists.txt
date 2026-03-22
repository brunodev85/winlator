cmake_minimum_required(VERSION 3.22.1)

project(PatchElf)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")

include_directories(src)

add_library(patchelf SHARED
            src/patchelf.cc)

target_link_libraries(patchelf)
