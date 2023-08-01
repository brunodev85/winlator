#!/bin/bash

clear
echo "Winlator OBB Image Generator"	

rm -r imagefs
mkdir -p imagefs

echo "Unzipping ubuntu-prepared.zip..."	
unzip -q -o ubuntu-prepared.zip -d imagefs

echo "Unzipping wine.zip..."	
mkdir -p imagefs/opt/wine
unzip -q -o wine.zip -d imagefs/opt/wine

echo "Unzipping patches.zip..."	
unzip -q -o patches.zip -d imagefs

cd imagefs
rm -r .l2s

echo "Creating OBB Image..."
tar -I 'zstd -19' -cf main.1.com.winlator.obb .

cp main.1.com.winlator.obb ../

cd ..
rm -r imagefs