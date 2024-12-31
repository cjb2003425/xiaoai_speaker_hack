#!/bin/sh

LOG_TMP_FILE="/tmp/mico.log"
LOG_TAR_FILE="/tmp/log.tar.gz"
LOG_TAR_FILE_LOWMEM="/data/log/log.tar.gz"
CURR_LOG="/var/log/messages"
LOG_MIIO_FILE="/tmp/log/miio.log"
[ -d /data/status/ ] && STATUS_LOG="/data/status/*"
[ -f /tmp/vlc-probe-data.log ] && VLC_PLOG="/tmp/vlc-probe-data.log"
[ -d /tmp/log/io.agora.rtc_sdk ] && AGORA_LOG="/tmp/log/io.agora.rtc_sdk"
[ -d /data/log/low-power-dump ] && LOW_POWER_DUMP="/data/log/low-power-dump"
[ -d /data/bes2600_fw_log/ ] && WIFI_LOG="/data/bes2600_fw_log/*"
[ -d /data/log/gsensor ] && GSENSOR_LOG="/data/log/gsensor"
GZ_LOGS=""
CMCC_LOGS=""


LOG_TITLE=collect_log.sh
mico_log() {
    echo $*
    logger -t $LOG_TITLE[$$] -p 3 "$*"
}

do_collect_cmcc_log() {
    local CMCC_LOG_PATH="/data/cmcc_ims/"

    mico_log "collecting cmcc log"
    [ -d "$CMCC_LOG_PATH" ] || return 0
    ls $CMCC_LOG_PATH | grep -q "voip*.log" || return 

    CMCC_LOGS="/data/cmcc_ims/voip*.log"

    [ -d "$CMCC_LOG_PATH" ] && CMCC_LOGS="$CMCC_LOGS /data/andlink/*"
    return 0
}

print_rss_info() {
    local status_path=""
    for status_path in /proc/[0-9]*/status
    do
        < "$status_path" \
        awk -F '\t' '{
            if ($1=="VmRSS:")
                rss=$2
            if ($1=="Name:")
                name=$2
        } END {
            if (rss!="" && name!="")
            print rss"\t"name
        }'
    done
}

do_collect_info() {
    mico_log "collecting system info"
    echo "==========SYSTEM info==========" >> $LOG_TMP_FILE
    timeout -t 10 /usr/bin/micocfg >> $LOG_TMP_FILE
    echo "==========TOP info==========" >> $LOG_TMP_FILE
    top -b -n1 >> $LOG_TMP_FILE

    echo "==========ps info==========" >> $LOG_TMP_FILE
    ps -w >> $LOG_TMP_FILE

    echo "==========mem info==========" >> $LOG_TMP_FILE
    free >> $LOG_TMP_FILE

    echo "========proc mem info=======" >> $LOG_TMP_FILE
    print_rss_info | sort -n -r | awk -F '\t' '{printf "%-16s" "%.2fMB\n",$2,$1/1024}' >> $LOG_TMP_FILE

    echo "==========uptime===========" >> $LOG_TMP_FILE
    uptime >> $LOG_TMP_FILE

    echo "==========df -h==============" >> $LOG_TMP_FILE
    df -h >> $LOG_TMP_FILE

    echo "==========tmp dir==========" >> $LOG_TMP_FILE
    ls -lh /tmp/ >> $LOG_TMP_FILE
    du -sh /tmp/* >> $LOG_TMP_FILE

    echo "==========data dir==========" >> $LOG_TMP_FILE
    ls -lh /data/ >> $LOG_TMP_FILE
    du -sh /data/* >> $LOG_TMP_FILE

    echo "==========ifconfig==========" >> $LOG_TMP_FILE
    ifconfig >> $LOG_TMP_FILE

    echo "==========wifi info==========" >> $LOG_TMP_FILE
    wl status >> $LOG_TMP_FILE
    wpa_cli status >> $LOG_TMP_FILE
    wl rate >> $LOG_TMP_FILE

    echo "==========traceroute speech==========" >> $LOG_TMP_FILE
    traceroute -T -p 80 -w 1 speech.ai.xiaomi.com >> $LOG_TMP_FILE

    echo "==========meminfo=========" >> $LOG_TMP_FILE
    cat /proc/meminfo >> $LOG_TMP_FILE


    echo "==========amixer=========" >> $LOG_TMP_FILE
    amixer >> $LOG_TMP_FILE
    echo "==========alsactl output=========" >> $LOG_TMP_FILE
    alsactl -f /tmp/alsactl.tmp store
    cat /tmp/alsactl.tmp >> $LOG_TMP_FILE
    rm -f /tmp/alsactl.tmp

    echo "==========dmesg===========:" >> $LOG_TMP_FILE
    dmesg >> $LOG_TMP_FILE
    echo "==========dmesg end===========:" >> $LOG_TMP_FILE

    [ -e /proc/slabinfo ] && {
        echo "==========slabinfo========"  >> $LOG_TMP_FILE
        cat /proc/slabinfo >> $LOG_TMP_FILE
    }

}

list_messages_gz(){
    for file in `ls /data/log/ | grep -E "^messages\.[0-9]*\.gz$|wireless.gz"`; do
        GZ_LOGS=${GZ_LOGS}" /data/log/"${file}
        done
}

do_clean_up() {
    rm -f $LOG_TAR_FILE
    [ -f $LOG_TAR_FILE_LOWMEM ] && rm -f $LOG_TAR_FILE_LOWMEM
}

do_collect_log() {
    mico_log "collecting system log"

    rm -f $LOG_TMP_FILE
    touch $LOG_TMP_FILE
    [ -f $LOG_TMP_FILE ] || {
        logger -s -p 1 -t logcollect "Failed to create temp log file"
        return
    }

    #hardware=`cat /proc/mico/model`

    do_collect_info
    list_messages_gz
    do_collect_cmcc_log

    [ ! -f  $LOG_MIIO_FILE ] && LOG_MIIO_FILE=""
    FILELIST="$LOG_TMP_FILE $CURR_LOG $GZ_LOGS $CMCC_LOGS $STATUS_LOG $LOG_MIIO_FILE $VLC_PLOG $AGORA_LOG $LOW_POWER_DUMP $WIFI_LOG $GSENSOR_LOG"

    # Are we low on memory?
    memfree=`cat /proc/meminfo  | grep MemFree | awk '{print $2}'`
    if [ "$memfree" -gt 2048 ]; then
        tar -zcf $LOG_TAR_FILE $FILELIST
    else
        tar -zcf $LOG_TAR_FILE_LOWMEM $FILELIST
        ln -s $LOG_TAR_FILE_LOWMEM $LOG_TAR_FILE
    fi

    rm -f $LOG_TMP_FILE
}

uploadfile_start()
{
    mico_log "collect $1 log start"
    ubus call mibrain text_to_speech "{\"text\":\"开始上传$2日志\",\"save\":0}"
}

uploadfile_success()
{
    mico_log "collect $1 log success"
    do_clean_up
    ubus call mibrain text_to_speech "{\"text\":\"$2日志上传成功\",\"save\":0}"
    sleep 2
}


uploadfile_fail()
{
    mico_log "collect $1 log fail"
    do_clean_up 
    ubus call mibrain text_to_speech "{\"text\":\"$2日志上传失败\",\"save\":0}"
    sleep 2
    
}

cmcc_upload()
{
    mico_log "collecting cmcc log"
    local EXTRA_NAME="和家固话"
    uploadfile_start cmcc $EXTRA_NAME
    /usr/bin/cmcc_helper -a uploadlog || { uploadfile_fail cmcc $EXTRA_NAME; return 1;}
    uploadfile_success cmcc $EXTRA_NAME

    do_collect_log
    /usr/bin/matool_upload_log $LOG_TAR_FILE lite
    do_clean_up
}

mico_upload() 
{
    mico_log "collecting mico log $1"
    do_collect_log ||  return 1
    /usr/bin/matool_upload_log /tmp/log.tar.gz lite $1 || return 1 ;
    return 0;
}

case "$1" in
    collect)
        do_collect_log
        ;;
    cleanup)
        do_clean_up
        ;;
    mico_upload)
        uploadfile_start mico
        mico_upload "$2"
        [ $? != 0 ] && { uploadfile_fail mico; return 0;}
        uploadfile_success mico
        ;;
    cmcc_upload)
        #mico_upload
        #echo aaa
        cmcc_upload
        ;;
    upload)
        mico_upload "$2"
        ;;
    *)
        ;;
esac

