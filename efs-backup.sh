#!/sbin/sh

#######################################
#  Do not remove this credits header  #
# sk8erwitskil : first release        #
# PhilZ-cwm6   : multi device support #
#######################################

EFS_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /efs | awk '{print $3}'`;

mkdir -p "$1"/clockworkmod/.efsbackup;

echo "">>"$1"/clockworkmod/.efsbackup/log.txt;
echo "Backup EFS ($EFS_PATH) to $1/clockworkmod/.efsbackup/efs.img">>"$1"/clockworkmod/.efsbackup/log.txt;
(cat "$EFS_PATH" > "$1"/clockworkmod/.efsbackup/efs.img) 2>> "$1"/clockworkmod/.efsbackup/log.txt;

if [ $? = 0 ];
     then echo "Success!">>$1/clockworkmod/.efsbackup/log.txt;
     else echo "Error!">>$1/clockworkmod/.efsbackup/log.txt;
fi;