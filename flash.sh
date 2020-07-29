#!/bin/bash

cd bootloader
if ! make flash; then
    exit 1
fi
cd ..
cd firmware
if ! make flash; then
    exit 1
fi
cd ..

