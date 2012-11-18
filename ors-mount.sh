#!/sbin/sh

#fix path to zip files specified by goomanager so that recovery knows where to find the files
#backup location is still /sdcard in all cases as goomanager doesn't add target path to backup command

MOUNT_INT=`cat /etc/fstab | grep k0p11 | awk '{print $2}'`;
MOUNT_EXT=`cat /etc/fstab | grep k1p1 | awk '{print $2}'`;
#valid for cwm6 by default:
# $MOUNT_INT should be /emmc on the s2 and most standard samsung phones (internal sd mount point: /dev/block/mmcblk0p11)
# $MOUNT_EXT should be /sdcard on the s2 (external sd mount point: /dev/block/mmcblk1p1)
# /etc/fstab file for the i9100 generated when recovery is running, based on recovery.fstab info
#/dev/block/mmcblk0p7 /cache ext4 rw
#/dev/block/mmcblk0p10 /data ext4 rw
#/dev/block/mmcblk0p11 /emmc vfat rw
#/dev/block/mmcblk0p9 /system ext4 rw
#/dev/block/mmcblk1p1 /sdcard vfat rw

if [ "$MOUNT_INT" = "/emmc" ] && [ "$MOUNT_EXT" = "/sdcard" ];
 then
    sed "s%/sdcard/external_sd/%/TMP_PATTERN/%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/sdcard/%/emmc/%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/TMP_PATTERN/%/sdcard/%g" -i /cache/recovery/openrecoveryscript;
#compatibilty if used outside PhilZ-cwm6 kernel with inversed fstab mount points in recovery.fstab:
elif [ "$MOUNT_INT" = "/sdcard" ] && [ "$MOUNT_EXT" = "/emmc" ];
 then sed "s%/sdcard/external_sd/%/emmc/%g" -i /cache/recovery/openrecoveryscript;
fi;
