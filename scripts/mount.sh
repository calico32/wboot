#!/bin/bash

set -ex

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <root image path> <mount point> <esp mount point>"
    exit 1
fi

ROOTIMG=$1
MOUNTPOINT=$2
ESP_MOUNTPOINT=$3

LOOPDEV=$(sudo losetup --find --show --partscan $ROOTIMG)

sudo mount ${LOOPDEV}p2 $MOUNTPOINT
sudo mkdir -p $MOUNTPOINT/boot
sudo mount ${LOOPDEV}p1 $ESP_MOUNTPOINT
sudo mount --bind $ESP_MOUNTPOINT $MOUNTPOINT/boot

echo $LOOPDEV > qemu/loopdev.txt
