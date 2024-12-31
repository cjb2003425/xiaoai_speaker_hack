#!/bin/sh
. /lib/functions/procd.sh

scan_ssid=$3
scan_if=$2
scan_type=$1
SCAN_RESULT_FILE="/tmp/wireless_wpa_scan_results"


WIRELESS_CONF_SCAN="/tmp/wpa_supplicant_scan.conf"
wireless_log "wpa scan prepare $scan_if"
ifconfig $scan_if up

[ ! -f $WIRELESS_CONF_SCAN ] && conf_prepare $WIRELESS_CONF_SCAN

[ "$(pidof wpa_supplicant)" == "" ] && /usr/sbin/wpa_supplicant -Dnl80211 -i$scan_if -c$WIRELESS_CONF_SCAN -s -dd &
wireless_log "wpa scan prepare $scan_if finish."

scan_id=$(wpa_cli -i $scan_if add_network)
wpa_cli -i $scan_if set_network $scan_id ssid \"$scan_ssid\"
wpa_cli -i $scan_if set_network $scan_id scan_ssid 1
#wpa_cli -i $scan_if set passive_scan 1 
[ "$scan_type" == "none" ] && wpa_cli -i $scan_if set_network $scan_id key_mgmt "NONE"
wpa_cli -i $scan_if list_networks 
wpa_cli -i $scan_if set filter_ssids 1
#wpa_cli -i $scan_if set passive_scan 0
wpa_cli -i $scan_if set ap_scan 1
wpa_cli -i $scan_if scan \"$scan_ssid\"
wpa_cli -i $scan_if enable_network $scan_id
#wpa_cli -i $scan_if set filter_rssi -70
sleep 5
wpa_cli -i $scan_if scan_result |sed '1d' > $SCAN_RESULT_FILE 
#wpa_cli -i $scan_if scan_result |sed '1d' > $SCAN_RESULT_FILE
wpa_cli -i $scan_if remove_network $scan_id 
wpa_cli -i $scan_if scan_result |sed '1d' > $SCAN_RESULT_FILE 
#cat $SCAN_RESULT_FILE |awk '{ printf($3" "$5"\n");}' |sort
#file2log $SCAN_RESULT_FILE 

#file2log $SCAN_RESULT_FILE 
#[ wc -l $SCAN_RESULT_FILE -gt 0 ] && return 1
awk -v ssid=$3 '{if($5==ssid) printf($1" "$2" "$3"\n");}' $SCAN_RESULT_FILE

