#!/sbin/sh

#######################################
#  Do not remove this credits header  #
# sk8erwitskil : first release        #
# PhilZ-cwm6   : multi device support #
#######################################

EFS_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /efs | awk '{print $3}'`;

echo "">>"$1"/clockworkmod/.efsbackup/log.txt;
echo "Restore $1/clockworkmod/.efsbackup/efs.img to $EFS_PATH">>"$1"/clockworkmod/.efsbackup/log.txt;
(cat "$1"/clockworkmod/.efsbackup/efs.img > "$EFS_PATH") 2>> "$1"/clockworkmod/.efsbackup/log.txt;

if [ $? = 0 ];
     then echo "Success!">>"$1"/clockworkmod/.efsbackup/log.txt;
     else echo "Error!">>"$1"/clockworkmod/.efsbackup/log.txt;
fi;