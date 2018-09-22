#!/bin/bash

# install dependencies (apt)
sudo apt-get update && sudo apt-get install g++ libssh-dev

# number of cores available
cores=`grep --count ^processor /proc/cpuinfo`

# clean first
make clean

# make project & install libs
make -j $cores && sudo make install