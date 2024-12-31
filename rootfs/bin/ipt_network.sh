#!/bin/sh
LOG_TITLE=$0
mico_log() {
    logger -t $LOG_TITLE[$$] -p 3 "$*"
}

MIOT_IPT_TABLE=miot_provision
miot_network_block()
{
    mico_log "add iptables $MIOT_IPT_TABLE rule to disable traffic" >/dev/null 2>&1
    iptables -F $MIOT_IPT_TABLE  >/dev/null 2>&1
    iptables -N $MIOT_IPT_TABLE  >/dev/null 2>&1
    iptables -t filter -I $MIOT_IPT_TABLE -p udp -m multiport --sport 67,68 -j ACCEPT  >/dev/null 2>&1
    iptables -t filter -I $MIOT_IPT_TABLE -p udp -m multiport --dport 67,68 -j ACCEPT  >/dev/null 2>&1
    iptables -t filter -I $MIOT_IPT_TABLE -p udp --dport 54321 -j ACCEPT  >/dev/null 2>&1
    iptables -t filter -I $MIOT_IPT_TABLE -p tcp --sport 54322 -j ACCEPT  >/dev/null 2>&1
    iptables -t filter -I $MIOT_IPT_TABLE -p tcp --dport 54322 -j ACCEPT  >/dev/null 2>&1

    iptables -F INPUT  >/dev/null 2>&1
    iptables -P INPUT DROP  >/dev/null 2>&1
    iptables -A INPUT -j $MIOT_IPT_TABLE >/dev/null 2>&1
}

miot_network_unblock()
{
    iptables -F INPUT >/dev/null 2>&1
    iptables -P INPUT ACCEPT >/dev/null 2>&1
    iptables -F $MIOT_IPT_TABLE >/dev/null 2>&1
    mico_log "remove iptables $MIOT_IPT_TABLE rule to enable traffic" >/dev/null 2>&1
}

CMD=$1

case $CMD in
    miot_network_block)
        miot_network_block
    ;;
    miot_network_unblock)
        miot_network_unblock
    ;;
esac
