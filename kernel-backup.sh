#!/sbin/sh

#######################################
#  Do not remove this credits header  #
# sk8erwitskil : first release        #
# PhilZ-cwm6   : multi device support #
#######################################

BOOT_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /boot | awk '{print $3}'`;

mkdir -p "$1"/clockworkmod/.kernel_bak;
DATE=$(date +%Y%m%d_%H%M%S);

echo "">>"$1"/clockworkmod/.kernel_bak/log.txt;
echo "Backup kernel ($BOOT_PATH) to $1/clockworkmod/.kernel_bak/boot_$DATE.img">>"$1"/clockworkmod/.kernel_bak/log.txt;
(cat "$BOOT_PATH" > "$1"/clockworkmod/.kernel_bak/boot_"$DATE".img) 2>> "$1"/clockworkmod/.kernel_bak/log.txt;

if [ $? = 0 ];
     then echo "Success!">>"$1"/clockworkmod/.kernel_bak/log.txt
     else echo "Error!">>"$1"/clockworkmod/.kernel_bak/log.txt
fi;