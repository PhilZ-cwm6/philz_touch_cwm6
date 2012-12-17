#!/sbin/sh

#######################################
#  Do not remove this credits header  #
# sk8erwitskil : first release        #
# PhilZ-cwm6   : multi device support #
#######################################

MODEM_PATH=`cat /etc/recovery.fstab | grep -v "#" | grep /modem | awk '{print $3}'`;

echo "">>"$2"/clockworkmod/.modem_bak/log.txt;
echo "Flash modem $1 to $MODEM_PATH">>"$2"/clockworkmod/.modem_bak/log.txt;
(cat "$1" > "$MODEM_PATH") 2>> "$2"/clockworkmod/.modem_bak/log.txt;

if [ $? = 0 ];
     then echo "Success!">>"$2"/clockworkmod/.modem_bak/log.txt
     else echo "Error!">>"$2"/clockworkmod/.modem_bak/log.txt
fi;