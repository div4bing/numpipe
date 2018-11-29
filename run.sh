#!/bin/bash

sudo rmmod numpipe
sudo rm /dev/numpipe
make clean
make
sudo insmod numpipe.ko buffer_size=100
sudo mknod /dev/numpipe c 243 0
sudo chmod -R 777 /dev/numpipe
