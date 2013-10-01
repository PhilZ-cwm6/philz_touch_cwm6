#!/sbin/sh

#######################################
#  Do not remove this credits header  #
#  sk8erwitskil : first release       #
#  PhilZ        : maintained          #
#######################################

# in cm-10.2 voldmanaged devices
# internal sd is always /sdcard which is a symlink to /storage/sdcard0 or /data/media (no more /emmc)
# external sd is now /storage/sdcard1, no more /external_sd
INT_SD=`cat /etc/recovery.fstab | grep -v "#" | grep -o /storage/sdcard0`;
EXT_SD=`cat /etc/recovery.fstab | grep -v "#" | grep -o /storage/sdcard1`;

# it could be non voldmanaged volume or /data/media: assume internal sd is /sdcard as it should always be (no more /emmc as internal)
if [ "$INT_SD" = "" ];
     then
        INT_SD="/sdcard"
fi;
if [ "$EXT_SD" = "" ];
     then
        EXT_SD="/sdcard"
fi;

#fix path for install zip and restore commands specified by goomanager so that recovery knows where to find the files
sed "s%/mnt/sdcard/external_sd%/EXT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
sed "s%/sdcard/external_sd%/EXT_PATTERN%g" -i /cache/recovery/openrecoveryscript;

sed "s%/mnt/storage/extSdCard%/EXT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
sed "s%/storage/extSdCard%/EXT_PATTERN%g" -i /cache/recovery/openrecoveryscript;

sed "s%/mnt/extSdCard%/EXT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
sed "s%/extSdCard%/EXT_PATTERN%g" -i /cache/recovery/openrecoveryscript;

sed "s%/mnt/storage/sdcard0%/INT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
sed "s%/storage/sdcard0%/INT_PATTERN%g" -i /cache/recovery/openrecoveryscript;

sed "s%/mnt/sdcard0%/INT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
sed "s%/sdcard0%/INT_PATTERN%g" -i /cache/recovery/openrecoveryscript;

sed "s%/mnt/sdcard%/INT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
sed "s%/sdcard%/INT_PATTERN%g" -i /cache/recovery/openrecoveryscript;

sed "s%/EXT_PATTERN%"$EXT_SD"%g" -i /cache/recovery/openrecoveryscript;
sed "s%/INT_PATTERN%"$INT_SD"%g" -i /cache/recovery/openrecoveryscript;
