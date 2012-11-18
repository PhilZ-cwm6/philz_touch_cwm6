#!/sbin/sh

BOOT_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /boot | awk '{print $3}'`;

mkdir -p $1/clockworkmod/.kernel_bak;
DATE=$(date +%Y%m%d_%H%M%S);

dd if=$BOOT_PATH of=$1/clockworkmod/.kernel_bak/boot_$DATE.img;
echo "Kernel ($BOOT_PATH) backed up to $1/clockworkmod/.kernel_bak/boot_$DATE.img">>$1/clockworkmod/.kernel_bak/log.txt;