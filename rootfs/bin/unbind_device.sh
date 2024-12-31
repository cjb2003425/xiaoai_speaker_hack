#!/bin/sh
LOG_TITLE=$0
mico_log() {
    logger -t $LOG_TITLE[$$] -p 3 "$*"
}

[ -f /tmp/unbind_in_process ] && { 
    mico_log "unregister in process"
    return
}
touch /tmp/unbind_in_process

mico_log "unregister device & reboot"

register_type=$(cat /data/status/register_type)
#only ble type not start cmcc, 
#if value empty or empty, start cmcc.
#for lx01 and lx05a
[ x"$register_type" != x"mico" -a -f /etc/init.d/cmcc_ims ] && {
    mico_log "register type not mico, try unregister cmcc"
    ubus -t 1 call voip cmcc_unregister '{"type":"999"}'
    sleep 8
    #for lx05a
    [ x"$register_type" == x"andlink" ] && {
        voip_log_list=$(ls /data/cmcc_ims)
        for one_log_file in $voip_log_list
        do
            mico_log "/data/cmcc_ims/"$one_log_file
            fsync "/data/cmcc_ims/"$one_log_file
        done
        mv /data/cmcc_ims /tmp/
    }
}
 
/usr/bin/mphelper pause

BOARD_TYPE=$(micocfg_board_id 2>/dev/null)
if [[ x"$BOARD_TYPE" = x"3" ]] || [[ x"$BOARD_TYPE" = x"6" ]] || [[ x"$BOARD_TYPE" = x"9" ]]; then
    mico_log "bluetooth stop"
    /etc/init.d/bluetooth stop
fi

mico_log "wireless stop"
/etc/init.d/wireless stop

#miplayer -f /usr/share/sound/shutdown.mp3
ubus -t 1 call qplayer play {\"play\":\"/usr/share/common_sound/shutdown.opus\"}

rm -r -f /data/* > /dev/null 2>&1
rm -r -f /data/.* >/dev/null 2>&1

#for lx05a
[ x"$register_type" == x"andlink" ] && {
    mv /tmp/cmcc_ims /data/
}

rm /tmp/unbind_in_process

mico_log "reboot..."

sync
sleep 1
reboot -f
