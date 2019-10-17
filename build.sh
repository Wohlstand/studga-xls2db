#!/bin/bash

#
#   How to install latest GCC on CentOS 7
#

# sudo yum install centos-release-scl
# sudo yum install devtoolset-7-gcc*
# scl enable devtoolset-7 bash
# which gcc
# gcc --version

source ./build_config.sh

if [ ! -d build ]; then
    mkdir build
fi

cd build

cmake -DCMAKE_BUILD_TYPE=Release -DSD_ROOTDIR="${SD_ROOTDIR}" -DSD_LOG_PATH="${SD_LOG_PATH}" ..
make -j 4

cd ..

