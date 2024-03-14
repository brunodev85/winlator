cmake_minimum_required(VERSION 3.22.1)

project(winlator)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-unused-function")

add_subdirectory(OpenXR-SDK)

add_library(winlator SHARED
            xr/engine.c
            xr/framebuffer.c
            xr/input.c
            xr/main.c
            xr/math.c
            xr/renderer.c
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
