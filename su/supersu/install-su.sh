#!/sbin/sh

set_perm() {
	chown $1.$2 $4
	chown $1:$2 $4
	chmod $3 $4
}

ch_con() {
	/system/bin/toolbox chcon u:object_r:system_file:s0 $1
	chcon u:object_r:system_file:s0 $1
}

mount /system
mount /data
mount -o rw,remount /system
mount -o rw,remount /system /system
mount -o rw,remount /
mount -o rw,remount / /

ARCH=arm
BIN=/sbin/supersu/$ARCH
COM=/sbin/supersu/common

chattr -i /system/xbin/su
chattr -i /system/bin/.ext/.su
chattr -i /system/xbin/daemonsu
chattr -i /system/etc/install-recovery.sh

rm -f /system/bin/su
rm -f /system/xbin/su
rm -f /system/xbin/daemonsu
rm -f /system/bin/.ext/.su
rm -f /system/etc/install-recovery.sh
rm -f /system/etc/init.d/99SuperSUDaemon
rm -f /system/etc/.installed_su_daemon
rm -f /system/app/Superuser.apk
rm -f /system/app/Superuser.odex
rm -f /system/app/SuperUser.apk
rm -f /system/app/SuperUser.odex
rm -f /system/app/superuser.apk
rm -f /system/app/superuser.odex
rm -f /system/app/Supersu.apk
rm -f /system/app/Supersu.odex
rm -f /system/app/SuperSU.apk
rm -f /system/app/SuperSU.odex
rm -f /system/app/supersu.apk
rm -f /system/app/supersu.odex
rm -f /data/dalvik-cache/*com.noshufou.android.su*
rm -f /data/dalvik-cache/*com.koushikdutta.superuser*
rm -f /data/dalvik-cache/*com.mgyun.shua.su*
rm -f /data/dalvik-cache/*Superuser.apk*
rm -f /data/dalvik-cache/*SuperUser.apk*
rm -f /data/dalvik-cache/*superuser.apk*
rm -f /data/dalvik-cache/*eu.chainfire.supersu*
rm -f /data/dalvik-cache/*Supersu.apk*
rm -f /data/dalvik-cache/*SuperSU.apk*
rm -f /data/dalvik-cache/*supersu.apk*
rm -f /data/dalvik-cache/*.oat
rm -f /data/app/com.noshufou.android.su-*
rm -f /data/app/com.koushikdutta.superuser-*
rm -f /data/app/com.mgyun.shua.su-*
rm -f /data/app/eu.chainfire.supersu-*

mkdir /system/bin/.ext
cp $BIN/su /system/xbin/daemonsu
cp $BIN/su /system/xbin/su
cp $BIN/su /system/bin/.ext/.su
cp $COM/install-recovery.sh /system/etc/install-recovery.sh
cp $COM/99SuperSUDaemon /system/etc/init.d/99SuperSUDaemon
echo 1 > /system/etc/.installed_su_daemon

set_perm 0 0 0777 /system/bin/.ext
set_perm 0 0 06755 /system/bin/.ext/.su
set_perm 0 0 06755 /system/xbin/su
set_perm 0 0 0755 /system/xbin/daemonsu
set_perm 0 0 0755 /system/etc/install-recovery.sh
set_perm 0 0 0755 /system/etc/init.d/99SuperSUDaemon
set_perm 0 0 0644 /system/etc/.installed_su_daemon

ch_con /system/bin/.ext/.su
ch_con /system/xbin/su
ch_con /system/xbin/daemonsu
ch_con /system/etc/install-recovery.sh
ch_con /system/etc/init.d/99SuperSUDaemon
ch_con /system/etc/.installed_su_daemon

/system/xbin/su --install

umount /system
umount /data

exit 0
