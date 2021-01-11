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
if [ "$1" = "label" ]; then
    ssh pannetra@paris bin/label-product.sh PiVoyager
fi
