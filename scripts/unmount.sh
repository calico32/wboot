#!/bin/bash

set -ex

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <mount point> <esp mount point>"
    exit 1
fi

MOUNTPOINT=$1
ESP_MOUNTPOINT=$2
LOOPDEV=$(cat qemu/loopdev.txt)

sudo umount -l $MOUNTPOINT/boot || true
sudo umount -l $MOUNTPOINT
sudo umount -l $ESP_MOUNTPOINT

sudo losetup -d $LOOPDEV

rm qemu/loopdev.txt
