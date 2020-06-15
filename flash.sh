#!/bin/bash

cd bootloader
make flash
cd ..
cd firmware
make flash
cd ..
