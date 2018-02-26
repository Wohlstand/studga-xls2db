#!/bin/bash

#
#   How to install latest GCC on CentOS 7
#

# sudo yum install centos-release-scl
# sudo yum install devtoolset-7-gcc*
# scl enable devtoolset-7 bash
# which gcc
# gcc --version

if [ ! -d build ]; then
    mkdir build
fi
cd build
export CC=/opt/rh/devtoolset-7/root/usr/bin/gcc
export CXX=/opt/rh/devtoolset-7/root/usr/bin/g++
cmake -DCMAKE_BUILD_TYPE=Release - ..
make -j 4
cd ..

