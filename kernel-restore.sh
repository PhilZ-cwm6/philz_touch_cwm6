#!/sbin/sh

BOOT_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /boot | awk '{print $3}'`;

echo "">>"$2"/clockworkmod/.kernel_bak/log.txt;
echo "Flash kernel $1 to $BOOT_PATH">>"$2"/clockworkmod/.kernel_bak/log.txt;
(cat "$1" > "$BOOT_PATH") 2>> "$2"/clockworkmod/.kernel_bak/log.txt;

if [ $? = 0 ];
     then echo "Success!">>"$2"/clockworkmod/.kernel_bak/log.txt
     else echo "Error!">>"$2"/clockworkmod/.kernel_bak/log.txt
fi;