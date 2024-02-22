cmake_minimum_required(VERSION 3.22.1)

project(winlator)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-unused-function")

add_subdirectory(OpenXR-SDK)

add_library(winlator SHARED
            xr/base.cpp
            xr/framebuffer.cpp
            xr/input.cpp
            xr/main.cpp
            xr/math.cpp
            xr/renderer.cpp
            drawable.c
            gpu_image.c
            sysvshared_memory.c
            xconnector_epoll.c)

target_link_libraries(winlator
                      log
                      android
                      jnigraphics
                      openxr_loader
                      EGL
                      GLESv2
                      GLESv3)
