#!/sbin/sh

mkdir -p $1/clockworkmod/.kernel_bak
dd if=/dev/block/mmcblk0p5 of=$1/clockworkmod/.kernel_bak/boot.img
