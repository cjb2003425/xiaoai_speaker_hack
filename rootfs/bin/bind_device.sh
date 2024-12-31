#!/bin/sh

export LED_PARENT=$0
LOG_TITLE=$0
mico_log() {
    logger -t $LOG_TITLE[$$] -p 3 "$*"
}

set_bind_and_restart_service() {
    sleep 20

    # check uid before bind
    local uid=$(micocfg_uid)
    [ x"$uid" == x"-1" -o x"$uid" == x"" ] && exit 1
    [ -f /tmp/unbind_in_process ] && exit 1

    mico_log "set bind by messageagent"
    ubus call mibt ble '{"action":"hidden"}'
    [ ! -f "/data/status/config_done" ] && {
        mico_log "set bind by messageagent　create config_done"
        mkdir -p /data/status
        touch /data/status/config_done
        sync
        /bin/shut_led 6
#do not notify
    }
}

case $1 in
*)
set_bind_and_restart_service
matool_download_miot_token sync
;;
esac
