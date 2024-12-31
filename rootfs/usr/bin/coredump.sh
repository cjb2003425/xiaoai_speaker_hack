#!/bin/sh

MAX_CORE_GZ_SIZE=4194304
LOGFILE="/var/log/messages"


pid="$1"
sig="$2"
prog="$3"

stat="$3""|""$2"
logger stat_points_none crash_app="$stat"

# skip coredump if same core already uploaded
[ -e /tmp/.core_history/"$prog"_"$sig" ] && exit 0

# print mem maps to log
echo "$prog crashed. print maps" >> $LOGFILE
cat /proc/"$pid"/maps >> $LOGFILE

#channel=`micocfg_channel`
#[ "$channel" = "release" ] && exit 0

[ -e /tmp/skip_core ] && exit 0

#try core dump
tmpfree=`df | grep -w "/tmp" | awk '{print $4}'`
memfree=`free | grep Mem | awk '{print $4}'`
[ "$tmpfree" -lt "16384" -a "$memfree" -lt "16384" ] && {
	logger -s -p 3 -t "coredump" "Memory low, skip dump."
	exit 0
}

cfile="core."$prog"."$sig".gz"
dfile="core."$prog"."$sig".tar.gz"
/bin/gzip > /tmp/"$cfile"
tar -czf /tmp/"$dfile" /tmp/"$cfile" "$LOGFILE"

dsize=`stat -c %s "$dfile"`
[ "$dsize" -gt "$MAX_CORE_GZ_SIZE" ] && {
	logger -s -p 3 -t "coredump" "Core file too large ["$dsize" bytes], skip."
	rm -f /tmp/"$cfile"
	rm -f /tmp/"$dfile"
	exit 0
}

/usr/bin/mtd_crash_log -a /tmp/"$dfile"
if [ $? -eq 0 ];then
mkdir -p /tmp/.core_history/                                                      
touch /tmp/.core_history/"$prog"_"$sig" 
fi

rm -f /tmp/"$cfile"
rm -f /tmp/"$dfile"

