#!/bin/bash

# cd src/
#source /mnt/HDD/Project/qt-kobo/koxtoolchain/refs/x-compile.sh kobo env
# /mnt/HDD/Project/qt-kobo/x-tools/arm-kobo-linux-gnueabihf/bin/arm-kobo-linux-gnueabihf-g++ -o ../inkbox-power-deamon main.cpp

export PATH=$PATH:/mnt/HDD/Project/qt-kobo/x-tools/arm-kobo-linux-gnueabihf/bin
mkdir build
cd build
cmake ../ -DCMAKE_TOOLCHAIN_FILE=../kobo.cmake
cmake --build .
cd ..

servername="root@10.42.0.28"
passwd="root"

sshpass -p $passwd ssh $servername "bash -c \"ifsctl mnt rootfs rw\""
sshpass -p $passwd ssh $servername "bash -c \"rm /inkbox-power-deamon\""
sshpass -p $passwd scp build/inkbox-power-deamon $servername:/

sshpass -p $passwd ssh $servername "bash -c \"sync\""

# Normal launch
#sshpass -p $passwd ssh $servername "bash -c \"DEBUG=true /inkbox-power-deamon\""

# For chroot
sshpass -p $passwd ssh $servername "bash -c \"rm /kobo/inkbox-power-deamon\""
sshpass -p $passwd ssh $servername "bash -c \"mv /inkbox-power-deamon /kobo/\""
sshpass -p $passwd ssh $servername "chroot /kobo sh -c \"DEBUG=true /inkbox-power-deamon\""