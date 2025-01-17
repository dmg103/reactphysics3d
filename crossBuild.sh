#!/bin/bash
git checkout master

rm -r build/
rm -r bin/

#Build linux
mkdir build
mkdir bin
cd build
cmake .. 
make -j4
cd ..
mv ./build/libreactphysics3d.a ./build/libreactphysics3dLinux.a
cp -r ./build/libreactphysics3dLinux.a ./bin/

#Build windows
rm -r build/
mkdir build
cd build
cmake -D CMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ ..
make -j4
cd ..
mv ./build/libreactphysics3d.a ./build/libreactphysics3dWin.a
cp -r ./build/libreactphysics3dWin.a ./bin/

#Build switch
rm -r build/
mkdir build
cd build
cmake -D CMAKE_CXX_COMPILER=/opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++ .. -DREACT_BUILD_SWITCH=TRUE
make -j4
cd ..
mv ./build/libreactphysics3d.a ./build/libreactphysics3dSwitch.a
cp -r ./build/libreactphysics3dSwitch.a ./bin/




