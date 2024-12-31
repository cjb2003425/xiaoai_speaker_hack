#!/bin/sh

LOG_TITLE=$0
mico_log() {
    logger -t $LOG_TITLE[$$] -p 3 "$*"
}
[ -z "$1" ] && echo 'Error: should be called from udhcpc' && exit 1

[ -n "$broadcast" ] && BROADCAST="broadcast $broadcast"
[ -n "$subnet" ] && NETMASK="netmask $subnet"

setup_interface () {
    /sbin/ifconfig $interface $ip $BROADCAST $NETMASK

    if [ -n "$router" ] ; then
        while route del default gw 0.0.0.0 dev $interface ; do
            true
        done
    
        for i in $router ; do
            route add default gw $i dev $interface
        done
    fi
}

deconfig_interface() {
    /sbin/ifconfig $interface 0.0.0.0
}

miio_wireless_connected_event() {
    mico_log "ubus call miio dhcp {\"interface\":\"$interface\", \"type\":\"$1\"}"
    ubus call miio dhcp "{\"interface\":\"$interface\", \"type\":\"$1\"}"
}

case "$1" in
    deconfig)
        mico_log "[deconfig dhcp] release ip"
        deconfig_interface
        miio_wireless_connected_event "release"
    ;;
    renew)
        setup_interface
        mico_log "[renew dhcp]"
        miio_wireless_connected_event "renew"
    ;;
    bound)
        setup_interface
        mico_log "[dhcp get ip success.]"
        miio_wireless_connected_event "bound"
    ;;
esac
