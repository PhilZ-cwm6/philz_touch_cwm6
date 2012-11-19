#!/sbin/sh

EFS_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /efs | awk '{print $3}'`;

mkdir -p $1/clockworkmod/.efsbackup;

dd if=$EFS_PATH of=$1/clockworkmod/.efsbackup/efs.img bs=4096;
echo "EFS ($EFS_PATH) backed up to $1/clockworkmod/.efsbackup/efs.img">>$1/clockworkmod/.efsbackup/log.txt;