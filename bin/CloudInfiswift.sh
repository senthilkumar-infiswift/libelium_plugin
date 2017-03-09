#!/bin/bash
CONFIG_FILE='/mnt/lib/cfg/infiswift/setup.ini'
C_PATH='/mnt/lib/cfg/infiswift/'
C_EXE='CloudInfiswift_sync'

interval=$(awk -F "=" '/interval/ {print $2}' $CONFIG_FILE)

while [ 1 ]
do
    starttime=$(date +%s)

    cd $C_PATH
    ./$C_EXE

    endtime=$(date +%s)
    sleeptime=$(($interval-($endtime-$starttime)))
    if [ "$sleeptime" -le 0 ]
    then
        sleeptime=0
    fi
    sleep $sleeptime

done
