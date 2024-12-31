#!/bin/bash
clear

rm -r build64
mkdir build64
cd build64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cross-arm64.cmake
make -j8

cd ..

rm -r build
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cross-armhf.cmake
make -j8