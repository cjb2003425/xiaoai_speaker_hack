#!/bin/sh

ROM_TYPE=`micocfg_model`
ROM_TYPE=`echo $ROM_TYPE|tr '[A-Z]' '[a-z]'`
SN=`micocfg_sn | sed 's/\//_/g'`
KFILE="$SN"_panic
UFILE=""$KFILE".tar.gz"

[ -e /tmp/.core_history/kernel_crash ] && exit 0
[ -e /tmp/skip_core ] && exit 0

case "$ROM_TYPE" in
    lx01|lx05a)
        flag=`hexdump -n 2 -x /dev/by-name/crashlog | grep "5ab5"`
        if [ "x$flag" != "x" ]; then
            loglen=`hexdump -n 10 -d /dev/by-name/crashlog|head -n 1|awk -F"   " '{print $6}'`
            logheadlen=28
            loglen=`expr $loglen + $logheadlen`
            dd if=/dev/by-name/crashlog of=/tmp/$KFILE bs=$loglen count=1
            tar -zcf /tmp/"$UFILE" /tmp/$KFILE
            /usr/bin/mtd_crash_log -u /tmp/"$UFILE"
            rm -f /tmp/$KFILE
            rm -f /tmp/"$UFILE"
            dd if=/dev/zero of=/dev/by-name/crashlog bs=1024
            sync
            mkdir -p /tmp/.core_history/
            touch /tmp/.core_history/kernel_crash
        fi
        ;;
    s12a|lx05|lx06|l06a|l07a|l09a|j01a|l09b|l15a|m03a)
        if [ "x`ls -A /sys/fs/pstore`" != "x" ]; then
            tar -zcf /tmp/"$UFILE" /sys/fs/pstore/*
            /usr/bin/mtd_crash_log -u /tmp/"$UFILE"
            rm -f /sys/fs/pstore/*
            rm -f /tmp/"$UFILE"
            mkdir -p /tmp/.core_history/
            touch /tmp/.core_history/kernel_crash
        fi
        ;;
    *)
        echo "no match model..."
        ;;
esac

