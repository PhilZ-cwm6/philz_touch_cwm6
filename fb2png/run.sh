#!/bin/sh
#
# helper script for capture picture on device
#
# Kyan He <kyan.ql.he@gmail.com> @ Tue Feb 15 12:42:48 CST 2011
#
#

#ADB_OPTIONS=
PNG="/data/local/fbdump.png"

FB2PNG=fb2png

adb $ADB_OPTIONS shell mount /data
adb $ADB_OPTIONS shell mkdir -p /data/local
adb $ADB_OPTIONS push $FB2PNG /data/local
adb $ADB_OPTIONS shell chmod 777 /data/local
adb $ADB_OPTIONS shell /data/local/fb2png

adb $ADB_OPTIONS pull $PNG
adb $ADB_OPTIONS shell rm $PNG
