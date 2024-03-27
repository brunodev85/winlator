cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_C_FLAGS "-O2 -Wno-unused-function -Wimplicit-function-declaration")

add_subdirectory(proot)
add_subdirectory(virglrenderer)

add_library(winlator SHARED
            drawable.c
            gpu_image.c
            sysvshared_memory.c
            xconnector_epoll.c)

target_link_libraries(winlator
                      log
                      android
                      jnigraphics
                      EGL
                      GLESv2
                      GLESv3)