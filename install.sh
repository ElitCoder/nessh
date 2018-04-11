#!/bin/bash

# number of cores available
cores=`grep --count ^processor /proc/cpuinfo`

# clean first (sudo will ask for sudo now, and not after building)
sudo make clean

# make project
make -j $cores

# install libs and includes
sudo make install