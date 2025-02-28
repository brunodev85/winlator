#!/bin/bash
clear

rm -r build
mkdir build
cd build

cmake ..
make -j8