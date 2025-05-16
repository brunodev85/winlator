cmake_minimum_required(VERSION 3.22.1)

project(MIDIHandler)

set(JNILIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../jniLibs/arm64-v8a)

add_library(libc++_shared SHARED IMPORTED)
set_target_properties(libc++_shared PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libc++_shared.so)

add_library(libFLAC SHARED IMPORTED)
set_target_properties(libFLAC PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libFLAC.so)

add_library(libfluidsynth SHARED IMPORTED)
set_target_properties(libfluidsynth PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libfluidsynth.so)

add_library(libfluidsynth-assetloader SHARED IMPORTED)
set_target_properties(libfluidsynth-assetloader PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libfluidsynth-assetloader.so)

add_library(libgio-2.0 SHARED IMPORTED)
set_target_properties(libgio-2.0 PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libgio-2.0.so)

add_library(libglib-2.0 SHARED IMPORTED)
set_target_properties(libglib-2.0 PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libglib-2.0.so)

add_library(libgmodule-2.0 SHARED IMPORTED)
set_target_properties(libgmodule-2.0 PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libgmodule-2.0.so)

add_library(libgobject-2.0 SHARED IMPORTED)
set_target_properties(libgobject-2.0 PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libgobject-2.0.so)

add_library(libgthread-2.0 SHARED IMPORTED)
set_target_properties(libgthread-2.0 PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libgthread-2.0.so)

add_library(libinstpatch-1.0 SHARED IMPORTED)
set_target_properties(libinstpatch-1.0 PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libinstpatch-1.0.so)

add_library(liboboe SHARED IMPORTED)
set_target_properties(liboboe PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/liboboe.so)

add_library(libogg SHARED IMPORTED)
set_target_properties(libogg PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libogg.so)

add_library(libopus SHARED IMPORTED)
set_target_properties(libopus PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libopus.so)

add_library(libpcre SHARED IMPORTED)
set_target_properties(libpcre PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libpcre.so)

add_library(libpcreposix SHARED IMPORTED)
set_target_properties(libpcreposix PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libpcreposix.so)

add_library(libsndfile SHARED IMPORTED)
set_target_properties(libsndfile PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libsndfile.so)

add_library(libvorbis SHARED IMPORTED)
set_target_properties(libvorbis PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libvorbis.so)

add_library(libvorbisenc SHARED IMPORTED)
set_target_properties(libvorbisenc PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libvorbisenc.so)

add_library(libvorbisfile SHARED IMPORTED)
set_target_properties(libvorbisfile PROPERTIES IMPORTED_LOCATION ${JNILIBS_DIR}/libvorbisfile.so)

find_package(OpenMP REQUIRED)

add_library(midihandler SHARED
            midihandler.c)

target_include_directories(midihandler PRIVATE fluidsynth/include)

target_link_libraries(midihandler
                      libFLAC
                      libfluidsynth-assetloader
                      libgio-2.0
                      libglib-2.0
                      libgmodule-2.0
                      libgobject-2.0
                      libgthread-2.0
                      libinstpatch-1.0
                      liboboe
                      libogg
                      libopus
                      libpcre
                      libpcreposix
                      libsndfile
                      libvorbis
                      libvorbisenc
                      libvorbisfile
                      libfluidsynth
                      OpenMP::OpenMP_CXX
                      log)