cmake_minimum_required(VERSION 3.22.1)

project(VirGLRenderer)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wno-unused-function -Wimplicit-function-declaration")

include_directories(src
                    src/gallium/include
                    src/gallium/auxiliary
                    src/gallium/auxiliary/util
                    server)

add_library(virglrenderer SHARED
            src/iov.c
            src/vrend_blitter.c
            src/vrend_decode.c
            src/vrend_formats.c
            src/vrend_object.c
            src/vrend_renderer.c
            src/vrend_shader.c
            server/virgl_server.c
            server/virgl_server_shm.c
            server/virgl_server_renderer.c
            src/gallium/auxiliary/util/u_format.c
            src/gallium/auxiliary/util/u_format_table.c
            src/gallium/auxiliary/util/u_texture.c
            src/gallium/auxiliary/util/u_hash_table.c
            src/gallium/auxiliary/util/u_debug.c
            src/gallium/auxiliary/util/u_cpu_detect.c
            src/gallium/auxiliary/util/u_bitmask.c
            src/gallium/auxiliary/util/u_surface.c
            src/gallium/auxiliary/util/u_math.c
            src/gallium/auxiliary/util/u_debug_describe.c
            src/gallium/auxiliary/cso_cache/cso_cache.c
            src/gallium/auxiliary/cso_cache/cso_hash.c
            src/gallium/auxiliary/tgsi/tgsi_dump.c
            src/gallium/auxiliary/tgsi/tgsi_ureg.c
            src/gallium/auxiliary/tgsi/tgsi_build.c
            src/gallium/auxiliary/tgsi/tgsi_scan.c
            src/gallium/auxiliary/tgsi/tgsi_info.c
            src/gallium/auxiliary/tgsi/tgsi_parse.c
            src/gallium/auxiliary/tgsi/tgsi_text.c
            src/gallium/auxiliary/tgsi/tgsi_strings.c
            src/gallium/auxiliary/tgsi/tgsi_sanity.c
            src/gallium/auxiliary/tgsi/tgsi_iterate.c
            src/gallium/auxiliary/tgsi/tgsi_util.c
            src/gallium/auxiliary/tgsi/tgsi_transform.c
            src/gallium/auxiliary/os/os_misc.c)

target_link_libraries(virglrenderer
                      log
                      android
                      EGL
                      GLESv2
                      GLESv3)