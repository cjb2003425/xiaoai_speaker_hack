#!/bin/sh

statfile="/sys/auxin_det/status"
check_linein()
{
    if [ -f $statfile ]; then
        status=$(cat $statfile)
        if [ "aux_out" = "$status" ]; then
            echo "aux_out"
            return 0
        elif [ "aux_in" = "$status" ]; then
            echo "aux_in"
            return 1
        else
            return 0
        fi
    fi
    return 0
}

check_linein
if [ $? == 1 ]; then
    logger -p 3 -t "check_linein" -s "stat_points_none auxin_device_using=1"
fi

