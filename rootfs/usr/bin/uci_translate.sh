#!/bin/sh
mkdir -p /data/status/
flag_file="/data/status/uci_translated_flag"
[ -f $flag_file ] && return;
touch $flag_file
fsync $flag_file

[  -f "/data/etc/binfo"  ] && {
    cat /data/etc/binfo |awk -F "[ \']" '{if($1~/option/) printf tolower($2)" = \""$4"\";\n"}' > /data/etc/device.info
    fsync /data/etc/device.info
    sed -i '1i\#这个文件已经废弃，新文件位置 /data/etc/device.info' /data/etc/binfo
}

[ -f "/data/messagingagent/messaging" ] && {
    cat /data/messagingagent/messaging |awk -F "[ \']" '{if($1~/option/) printf tolower($2)" = \""$4"\";\n"}' > /data/etc/messaging.cfg
    fsync /data/etc/messaging.cfg
    sed -i '1i\#这个文件已经废弃，新文件位置 /data/etc/messaging.cfg' /data/messagingagent/messaging
}


