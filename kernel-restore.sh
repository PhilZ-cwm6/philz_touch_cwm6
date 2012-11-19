#!/sbin/sh

BOOT_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /boot | awk '{print $3}'`;

dd if=$1 of=$BOOT_PATH bs=4096;
echo "Kernel $1 flashed to $BOOT_PATH">>$2/clockworkmod/.kernel_bak/log.txt;
