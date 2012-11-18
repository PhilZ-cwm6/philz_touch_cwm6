#!/sbin/sh

EXT_SD=`cat /etc/recovery.fstab | grep -v "#" | grep -o /sdcard`;
if [ "$EXT_SD" = "" ];
     then EXT_SD="/external_sd"
fi;

#fix path to zip files specified by goomanager so that recovery knows where to find the files
#backup location is still /sdcard in all cases as goomanager doesn't add target path to backup command
#however, as of v2.1.2, goomanager cannot see extSdCard location

    sed "s%/sdcard/external_sd%/TMP_PATTERN%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/storage/extSdCard%/TMP_PATTERN%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/extSdCard%/TMP_PATTERN%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/storage/sdcard0%/emmc%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/sdcard0%/emmc%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/sdcard%/emmc%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/TMP_PATTERN%"$EXT_SD"%g" -i /cache/recovery/openrecoveryscript;
