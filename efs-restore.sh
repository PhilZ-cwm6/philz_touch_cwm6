#!/sbin/sh

EFS_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /efs | awk '{print $3}'`;

dd if=$1/clockworkmod/.efsbackup/efs.img of=$EFS_PATH;
echo "$1/clockworkmod/.efsbackup/efs.img restored to $EFS_PATH">>$1/clockworkmod/.efsbackup/log.txt;
