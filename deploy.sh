#!/bin/bash

# cd src/
# source /mnt/HDD/Project/qt-kobo/koxtoolchain/refs/x-compile.sh kobo env
# /mnt/HDD/Project/qt-kobo/x-tools/arm-kobo-linux-gnueabihf/bin/arm-kobo-linux-gnueabihf-g++ -o ../inkbox-power-deamon main.cpp

# Compile with this script. Do it.

# Edit it for scripts repo...

export PATH=$PATH:/home/build/qos/toolchains/armv7l-linux-musleabihf-cross/bin
rm -rf build
mkdir build
cd build
# https://clangd.llvm.org/installation.html#project-setup
cmake ../ -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_TOOLCHAIN_FILE=../kobo.cmake
cmake --build . --config Release -- -j 8
/home/build/qos/toolchains/armv7l-linux-musleabihf-cross/bin/armv7l-linux-musleabihf-strip ipd
chmod +x ipd
cd ..

#servername="root@10.42.0.28"
servername="root@192.168.2.2"
passwd="root"

sshpass -p $passwd ssh $servername "bash -c \"ifsctl mnt rootfs rw\""
sshpass -p $passwd ssh $servername "bash -c \"rm /ipd\""

sshpass -p $passwd ssh $servername "bash -c \"killall -9 ipd\""
sshpass -p $passwd ssh $servername "bash -c \"service ipd stop\""
sshpass -p $passwd ssh $servername "bash -c \"killall -9 ipd\""

# sshpass -p $passwd ssh $servername "bash -c \"service sleep_standby stop\""
# sshpass -p $passwd ssh $servername "bash -c \"killall -9 sleep_standby.sh\""
# sshpass -p $passwd ssh $servername "bash -c \"killall -9 evtest\""
# sshpass -p $passwd ssh $servername "bash -c \"killall -9 evtest\""
# sshpass -p $passwd ssh $servername "bash -c \"killall -9 evtest\""

sshpass -p $passwd scp build/ipd $servername:/

sshpass -p $passwd ssh $servername "bash -c \"sync\""

# Normal launch
# sshpass -p $passwd ssh $servername "bash -c \"DEBUG=true /inkbox-power-deamon\""

# For chroot
# sshpass -p $passwd ssh $servername "bash -c \"rm /kobo/inkbox-power-deamon\""
# sshpass -p $passwd ssh $servername "bash -c \"mv /inkbox-power-deamon /kobo/\""
# shpass -p $passwd ssh $servername "chroot /kobo sh -c \"DEBUG=true /inkbox-power-deamon\""

# Reconnect to wifi
# /usr/local/bin/wifi/turn_on_with_stats.sh
# /usr/local/bin/wifi/connect_to_network.sh hotspot 12345678 &
