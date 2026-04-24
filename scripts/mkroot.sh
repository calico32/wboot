#!/bin/bash

set -ex

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <root image path>"
    exit 1
fi

ROOTIMG=$1

touch $ROOTIMG
truncate -s 16G $ROOTIMG

sfdisk $ROOTIMG <<EOF
label: gpt
size=256M, type=uefi
size=, type=linux
EOF
sfdisk --part-label $ROOTIMG 1 "esp"
sfdisk --part-label $ROOTIMG 2 "root"

LOOPDEV=$(sudo losetup --find --show --partscan $ROOTIMG)

sudo mkfs.ext4 ${LOOPDEV}p2
sudo mkfs.fat -F 32 ${LOOPDEV}p1

sudo losetup -d $LOOPDEV
