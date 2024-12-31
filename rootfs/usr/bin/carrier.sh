#!/bin/sh
# minet_client -s 169.254.29.1 -m "34:CE:00:9D:FE:94" -d "60846786" -k "7oA2M2RasxqwSApe"
. /usr/share/libubox/jshn.sh

LOG_TITLE="carrier.sh"
mico_log() {
    logger -t $LOG_TITLE[$$] -p 3 "$*"
    echo $*
}


[ x"$1" == x"CTEI" ] && {
    mico_log "CTEI try start"
    [ -f /data/messagingagent/CTEI ] && {
        mico_log "CTEI  start carrier_chinatelecom.sh"
        ctei_pid=$(pidof carrier_chinatelecom.sh)
        [ -z $ctei_pid ] && {
            killall -9 carrier_chinatelecom.sh
            /usr/bin/carrier_chinatelecom.sh &
        }
    }
    return   
}

[ x"$1" == x"CMEI" ] && {
    mico_log "CMEI try start"
    custom_type=$(micocfg_custom)
    [ -f /data/messagingagent/CMEI -a x"${custom_type}" != x"CMCC" ] && {
        mico_log "CMEI  start cmcc_dm"
        /etc/init.d/cmcc_dm restart
    }
    mico_log "CMEI try start finish"
    return
}

STAT_FILE=/data/messagingagent/carrier
[ ! -f $STAT_FILE ] && {
    mico_log "status file $STAT_FILE not exist."
    return;
}

RECV_BUF=$(cat $STAT_FILE)
json_init
json_load "$RECV_BUF"
json_get_var sichuan_telecom SICHUAN_TELECOM
json_get_var zhejiang_telecom ZHEJIANG_TELECOM
json_cleanup

mico_log "sichuan telecom status $sichuan_telecom"
mico_log "zhejiang telecom status $zhejiang_telecom"

[ ! -z $sichuan_telecom ] && {
    if [ "$sichuan_telecom" == "1"  ] 
    then
        mico_log "sichuan telecom enable plugin"
        /etc/init.d/telecom_plugin enable
    else
        mico_log "sichuan telecom disable plugin"
        /etc/init.d/telecom_plugin disable
    fi
}

[ ! -z $zhejiang_telecom ] && {
    if [ "$zhejiang_telecom" == "1"  ] 
    then
        mico_log "zhejiang telecom enable plugin"
        /etc/init.d/telecom_zhejiang enable
    else
        mico_log "zhejiang telecom disable plugin"
        /etc/init.d/telecom_zhejiang disable
    fi
}



