#!/sbin/sh

dd if=$1/clockworkmod/.efsbackup/efs.img of=/dev/block/mmcblk0p1
