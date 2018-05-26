#!/bin/bash

# install dependencies (pacman)
sudo pacman -Syy && sudo pacman -S --needed gcc libssh

# number of cores available
cores=`grep --count ^processor /proc/cpuinfo`

# clean first
make clean

# make project & install libs
make -j $cores && sudo make install
