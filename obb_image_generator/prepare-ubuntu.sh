#!/bin/bash

# This script must be run from within rootfs

export PATH

clear

chmod -R 777 /tmp

dpkg --add-architecture armhf

apt update

apt install -y locales
locale-gen --no-archive en_US.utf8

#wine dependencies (armhf)
apt install -y libasound2:armhf libpulse0:armhf libc6:armhf libglib2.0-0:armhf libgstreamer1.0-0:armhf gstreamer1.0-plugins-base:armhf gstreamer1.0-plugins-good:armhf gstreamer1.0-plugins-ugly:armhf gstreamer1.0-libav:armhf libx11-6:armhf libxext6:armhf libfreetype6:armhf libglu1-mesa:armhf libosmesa6:armhf libxcomposite1:armhf libxcursor1:armhf libxfixes3:armhf libxi6:armhf libxrandr2:armhf libxrender1:armhf libxcb-randr0:armhf libxcb-xfixes0:armhf libwayland-client0:armhf libvulkan1:armhf libopenal1:armhf

#wine dependencies (arm64)
apt install -y libasound2:arm64 libpulse0:arm64 libc6:arm64 libglib2.0-0:arm64 libgstreamer1.0-0:arm64 gstreamer1.0-plugins-base:arm64 gstreamer1.0-plugins-good:arm64 gstreamer1.0-plugins-ugly:arm64 gstreamer1.0-libav:arm64 libx11-6:arm64 libxext6:arm64 libfreetype6:arm64 libglu1-mesa:arm64 libosmesa6:arm64 libxcomposite1:arm64 libxcursor1:arm64 libxfixes3:arm64 libxi6:arm64 libxrandr2:arm64 libxrender1:arm64 libxcb-randr0:arm64 libxcb-xfixes0:arm64 libwayland-client0:arm64 libvulkan1:arm64 libopenal1:arm64

apt autoremove -y
apt clean

cd /usr/lib/arm-linux-gnueabihf
rm -r dri
rm -r libLLVM-9.so
rm -r libLLVM-9.so.1

cd /usr/lib/aarch64-linux-gnu
rm -r dri
rm -r libLLVM-9.so
rm -r libLLVM-9.so.1

rm -r /root/prepare-ubuntu.sh
rm -r /var/lib/apt/lists/*

exit