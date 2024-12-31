#!/bin/sh
#####################CTEI
#LOG_TITLE="carrier.sh"
#mico_log() {
#    logger -t $LOG_TITLE[$$] -p 3 "$*"
#    echo $*
#}

. /usr/share/libubox/jshn.sh

LOG_TITLE="carrier_chinatelecom.sh"
mico_log() {
    logger -t $LOG_TITLE[$$] -p 3 "$*"
    echo $*
}
#http://smarthome.ctei.ott4china.com/lla/bt
china_telecom()
{
    local tmp_outfile="/tmp/china_telecom.output"
    CTEI=$(cat /data/messagingagent/CTEI)
    [ -z $CTEI ] && {
        mico_log "CTEI empty, no report"
        return;
    }
    local wan_ip=$(ifconfig wlan0 | grep "inet addr" | awk '{ print $2}'|awk -F':' '{print $2}')
    local wan_mac=$(micocfg_mac)
    local rom_version=$(micocfg_version)
    mico_log "CTEI $CTEI"
    mico_log "ip  $wan_ip"
    mico_log "mac $wan_mac"
    mico_log "version $rom_version"

    local update_date=$(date +%F" "%X)
    mico_log "date $update_date"
    
    for try_times in `seq 1 3`;
    do

        post_content="{\"VER\":\"01\",\"CTEI\":\"${CTEI}\",\"MAC\":\"${wan_mac}\",\"IP\":\"${wan_ip}\",\"UPLINKMAC\":\"$gw_mac\",\"LINK\":\"2\",\"FWVER\":\"${rom_version}\",\"DATE\":\"${update_date}\"}"
        mico_log "try $try_times times smart send:$post_content"
        curl -H "Content-Type: application/json" -X POST  --data '{"VER":"01","CTEI":"'${CTEI}'","MAC":"'${wan_mac}'","IP":"'${wan_ip}'","UPLINKMAC":"","LINK":"2","FWVER":"'${rom_version}'","DATE":"'$(date +%F)' '$(date +%X)'"}' "http://smarthome.ctei.ott4china.com/lla/bt" >$tmp_outfile
        #curl -v http://pdm.tydevice.com/?jsonstr=$post_content
        #wget -O $tmp_outfile "http://pdm.tydevice.com/?jsonstr=$post_content"
        #wget -O $tmp_outfile "http://smarthome.ctei.ott4china.com/lla/bt?jsonstr=$post_content"
        #{"CODE":-91000,"DESC":"传入参数错误"}
        json_init
        json_load "$(cat $tmp_outfile)"
        json_get_var ret_code "CODE"
        json_get_var ret_desc "DESC"
        json_cleanup

        mico_log "$(cat $tmp_outfile)"
        mico_log "CODE $ret_code"
        mico_log "DESC $ret_desc"
        [ "$ret_code" == "0" ] && {
            mico_log "report success ,wait 86400"
            break
        }
        sleep 10
    done

    for try_times in `seq 1 3`;
    do
        post_content="{\"VER\":\"01\",\"CTEI\":\"${CTEI}\",\"MAC\":\"24:e2:71:f4:d7:b0\",\"IP\":\"${wan_ip}\",\"UPLINKMAC\":\"$gw_mac\",\"LINK\":\"2\",\"FWVER\":\"${rom_version}\",\"DATE\":\"${update_date}\"}"
        mico_log "try $try_times times pdm send:$post_content"
        wget -O $tmp_outfile "http://pdm.tydevice.com/?jsonstr=$post_content"
        #{"CODE":-91000,"DESC":"传入参数错误"}
        json_init
        json_load "$(cat $tmp_outfile)"
        json_get_var errcode "errcode"
        json_get_var status_code "Status Code"
        json_get_var errmsg "errmsg"

        json_cleanup

        mico_log "$(cat $tmp_outfile)"
        mico_log "errcode $errcode"
        mico_log "status_code $status_code"
        mico_log "errmsg $errmsg"
        [ "$status_code" == "200" ] && {
            mico_log "report success ,wait 86400"
            break
        }
        sleep 10
    done
}

gw_mac_get()
{
    gw_ip=$(route |grep default|awk '{print $2}')
    [ -z $gw_ip ] && {
        return;
    }
     
    gw_mac=$(grep $gw_ip" "  /proc/net/arp |awk '{print $4}')
}

gw_mac_get

while true
do
    china_telecom
    #sleep 86400
    sleep 86400
done
