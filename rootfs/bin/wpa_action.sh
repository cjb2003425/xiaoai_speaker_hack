#!/bin/sh
LOG_TITLE=$0
mico_log() {
    logger -t $LOG_TITLE[$$] -p 3 "$*"
}

MIOT_HIDDEN_SSID="25c829b1922d3123_miwifi"

miio_wireless_disconnected_event() {
    [ `micocfg_miot_auto_provision_support` != "yes" ] && {
        mico_log "no need send wireless_disconnected_event event"
        return;
    }

    #[ ! -f "/data/status/config_done" ] && {
    #    logger -t network_probe.sh -p 3 "need send wireless_disconnected_event event, not config done"
    #    return
    #}

    ssid=`cat /tmp/wifi_connected_info.ssid 2>/dev/null`
    bssid=`cat /tmp/wifi_connected_info.bssid 2>/dev/null`
    mico_log "ubus call miio wifi_disconnected  {\"from\":\"$LOG_TITLE\",\"ssid\":\""$ssid"\",\"bssid\":\"$bssid\"}"
    #ubus call miio wifi_disconnected  {\"from\":\"$LOG_TITLE\",\"ssid\":\""$ssid"\",\"bssid\":\"$bssid\"} &
    micocfg_miio_wifi_disconnected  "$LOG_TITLE" "$ssid" "$bssid"  &

    /bin/ipt_network.sh miot_network_unblock
}

miio_wireless_connected_event() {
    [ `micocfg_miot_auto_provision_support` != "yes" ] && {
        mico_log "no need send wireless_connected_event event"
        return;
    }

    #[ ! -f "/data/status/config_done" ] && {
    #    logger -t network_probe.sh -p 3 "need send wireless_connected_event event, not config done"
    #    return
    #}

    ssid=`micocfg_ssid`
    bssid=`micocfg_bssid`

    echo -n "$ssid" > /tmp/wifi_connected_info.ssid
    echo -n "$bssid" > /tmp/wifi_connected_info.bssid

    if [ x"$ssid" == x"$MIOT_HIDDEN_SSID" ]; then
        /bin/ipt_network.sh miot_network_block
    else
        /bin/ipt_network.sh miot_network_unblock
    fi

    mico_log "save wifi info > /tmp/wifi_connected_info " \""$ssid"\" $bssid
}


IFNAME=$1
CMD=$2
echo "$CMD"  >> /tmp/wifi_event.log

if [ "$CMD" = "CONNECTED" ]; then
    mico_log "wpa connected"
    # configure network, signal DHCP client, etc.
    /etc/init.d/dhcpc start
    /etc/init.d/odhcp6c start
    miio_wireless_connected_event
fi

if [ "$CMD" = "DISCONNECTED" ]; then
    # remove network configuration, if needed
#    show_led 6
    mico_log "wpa event DISCONNECTED"
    /etc/init.d/dhcpc stop
    /etc/init.d/odhcp6c stop
    /etc/init.d/miplay stop
    miio_wireless_disconnected_event
fi
