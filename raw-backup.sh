#!/sbin/sh

#######################################
#  Do not remove this credits header  #
#  Raw backup support through shell   #
#  First Release: PhilZ-cwm6          #
#######################################

#usage: raw-backup.sh [-br] [path] [volume] [mount point]
#  -b   backup
#  -r   restore
#exp:   raw-backup.sh -b /sdcard/clockworkmod/.efsbackup/raw /dev/block/mmcblk0p1 /efs
#       raw-backup.sh -r /sdcard/clockworkmod/.efsbackup/raw/efs.img /dev/block/mmcblk0p1 /efs

if [ "$1" = "-b" ]
then
# start backup
    mkdir -p "$2";
    DATE=$(date +%Y%m%d_%H%M%S);

    echo "">>"$2"/log.txt;
    echo "Backup "$4" ("$3") to "$2""$4"_$DATE.img">>"$2"/log.txt;
    (cat "$3" > "$2""$4"_"$DATE".img) 2>> "$2"/log.txt;

    if [ $? = 0 ];
    then
       echo "Success!">>"$2"/log.txt;
       exit 0;
    else
      echo "Error!">>"$2"/log.txt;
      exit 1;
    fi;
# end backup

# start restore
elif [ "$1" = "-r" ]
then
    BACKUP_FOLDER=$(dirname "$2")
    echo "">>"$BACKUP_FOLDER"/log.txt;
    echo "Restore "$2" to "$3" ("$4")">>"$BACKUP_FOLDER"/log.txt;
    (cat "$2" > "$3") 2>> "$BACKUP_FOLDER"/log.txt;

    if [ $? = 0 ];
    then
       echo "Success!">>"$BACKUP_FOLDER"/log.txt;
       exit 0;
    else
      echo "Error!">>"$BACKUP_FOLDER"/log.txt;
      exit 1;
    fi;
# end restore

#bad call option
else exit 2;
fi;

