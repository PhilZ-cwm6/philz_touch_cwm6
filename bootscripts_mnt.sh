#!/sbin/sh

# PhilZ Touch boot script fix for storage paths
#  - openrecoveryscript support through GooManager
#  - extendedcommand support through ROM Manager

# in +cm-10.2 voldmanaged devices:
# internal sd is always /sdcard which is a symlink to /storage/sdcard0 or /data/media (no more /emmc)
# external sd is now /storage/sdcard1, no more /external_sd

BOOT_SCRIPT=$1
INT_SD=$2
EXT_SD="/storage/sdcard1";

# fix path for install zip and restore commands specified by goomanager so that recovery knows where to find the files
# also fix ROM Manager backup/restore to/from external_sd
sed "s%/mnt/sdcard/external_sd%/EXT_PATTERN%g" -i $BOOT_SCRIPT;
sed "s%/sdcard/external_sd%/EXT_PATTERN%g" -i $BOOT_SCRIPT;
sed "s%/external_sd%/EXT_PATTERN%g" -i $BOOT_SCRIPT;

sed "s%/mnt/storage/extSdCard%/EXT_PATTERN%g" -i $BOOT_SCRIPT;
sed "s%/storage/extSdCard%/EXT_PATTERN%g" -i $BOOT_SCRIPT;

sed "s%/mnt/extSdCard%/EXT_PATTERN%g" -i $BOOT_SCRIPT;
sed "s%/extSdCard%/EXT_PATTERN%g" -i $BOOT_SCRIPT;

sed "s%/mnt/storage/sdcard0%/INT_PATTERN%g" -i $BOOT_SCRIPT;
sed "s%/storage/sdcard0%/INT_PATTERN%g" -i $BOOT_SCRIPT;

sed "s%/mnt/sdcard0%/INT_PATTERN%g" -i $BOOT_SCRIPT;
sed "s%/sdcard0%/INT_PATTERN%g" -i $BOOT_SCRIPT;

sed "s%/mnt/sdcard%/INT_PATTERN%g" -i $BOOT_SCRIPT;
sed "s%/sdcard%/INT_PATTERN%g" -i $BOOT_SCRIPT;

sed "s%/EXT_PATTERN%"$EXT_SD"%g" -i $BOOT_SCRIPT;
sed "s%/INT_PATTERN%"$INT_SD"%g" -i $BOOT_SCRIPT;
