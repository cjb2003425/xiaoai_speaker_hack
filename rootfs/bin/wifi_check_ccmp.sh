#!/bin/sh

mico_log() {
    logger -t wifi_check_ccmp.sh -p 3 "$*"
}

_dhcpcheck=0
_dnscheck=0

while true;
do
    sleep 5
    ccmperr=`wl counters | grep ccmpundec | awk '{print $2}'`
    if [ "$ccmperr" != "" -a "$ccmperr" != "0" ]; then
        mico_log "restart wifi find ccmp err"
        echo ccmperr > /tmp/wifi_check_ccmp_err
        /etc/init.d/wireless restart "wificheck"
    fi
done
