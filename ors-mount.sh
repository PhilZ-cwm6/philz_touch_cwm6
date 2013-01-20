#!/sbin/sh

#######################################
#  Do not remove this credits header  #
# sk8erwitskil : first release        #
# PhilZ-cwm6   : multi device support #
# PhilZ-cwm6   : Jellybean support    #
# PhilZ-cwm6   : No extsdcard support #
#######################################

#in cwm, sdcard can be internal sd or external sd on phones with /emmc and no /external_sd defined in recovery.fstab
INT_SD=`cat /etc/recovery.fstab | grep -v "#" | grep -o /emmc`;
EXT_SD=`cat /etc/recovery.fstab | grep -v "#" | grep -o /external_sd`;

if [ "$INT_SD" = "" ];
     then
        INT_SD="/sdcard"
fi;
if [ "$EXT_SD" = "" ];
     then
        EXT_SD="/sdcard"
fi;

#fix path to zip files specified by goomanager so that recovery knows where to find the files
#backup location is still /sdcard in all cases as goomanager doesn't add target path to backup command
#however, as of v2.1.2, goomanager cannot see extSdCard location
#samsung: /sdcard is always internal sd

    sed "s%/sdcard/external_sd%/EXT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/storage/extSdCard%/EXT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/extSdCard%/EXT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/storage/sdcard0%/INT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/sdcard0%/INT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/sdcard%/INT_PATTERN%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/EXT_PATTERN%"$EXT_SD"%g" -i /cache/recovery/openrecoveryscript;
    sed "s%/INT_PATTERN%"$INT_SD"%g" -i /cache/recovery/openrecoveryscript;
