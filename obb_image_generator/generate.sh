#!/bin/bash

OBB_IMG_VERSION=3

clear
echo "Winlator OBB Image Generator"	

if [ -d imagefs ]; then
	rm -r imagefs
fi

mkdir -p imagefs

echo "Extracting ubuntu-prepared.txz..."	
tar -xf ubuntu-prepared.txz -C imagefs

echo "Extracting wine.txz..."
tar -xf wine.txz -C imagefs/opt

for f in imagefs/opt/*
do
	if [[ $f == *wine* ]]
	then
		mv $f imagefs/opt/wine
	fi
done

echo "Extracting patches.txz..."	
tar -xf patches.txz -C imagefs

echo "Creating OBB Image..."
tar -I 'zstd -19' -cf main.$OBB_IMG_VERSION.com.winlator.obb -C imagefs .

rm -r imagefs