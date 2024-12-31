#!/bin/sh
logger -t dnsmasq_action.sh -p 3 "ap dhcp mac:$DNSMASQ_CLIENT_ID name:$DNSMASQ_SUPPLIED_HOSTNAME"
[ `micocfg_miot_auto_provision_support` == "yes" ] && {
    logger -t dnsmasq_action.sh -p 3 "send miio ap_connected event"
    ubus call miio ap_connected
}
