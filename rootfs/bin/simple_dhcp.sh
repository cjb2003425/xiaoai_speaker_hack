#!/bin/sh

MICO_SYSLOG_BINDDEVICE="[MICOREGISTER] "
MICO_SYSLOG_PERFORMANCE="[performance] "

MIOT_HIDDEN_SSID="25c829b1922d3123_miwifi"

export LED_PARENT=simple_dhcp.sh
LOG_TITLE=$0
mico_log() {
    logger -t $LOG_TITLE[$$] -p 3 "$*"
}
[ -z "$1" ] && echo 'Error: should be called from udhcpc' && exit 1

[ -n "$broadcast" ] && BROADCAST="broadcast $broadcast"
[ -n "$subnet" ] && NETMASK="netmask $subnet"

RESOLV_CONF="/tmp/resolv.conf.auto"
RESOLV_CONF_TMP="/tmp/resolv.conf.auto.tmp"

setup_predef_dns() {
    DNS_SERVERS_CN="180.76.76.76 223.5.5.5 223.6.6.6"
    DNS_SERVERS_CN=$(echo $DNS_SERVERS_CN |awk 'BEGIN{srand()}{for(i=1;i<=NF;i++) b[rand()NF]=$i}END{for(x in b)printf "%s ",b[x]}')
    dns=$DNS_SERVERS_CN

    echo -n > $RESOLV_CONF_TMP

    mico_log "dns list $dns"
    for i in $dns ; do
        echo nameserver $i >> $RESOLV_CONF_TMP
    done

    cp $RESOLV_CONF_TMP $RESOLV_CONF -f >> /dev/null
}

setup_dns() {
    echo -n > $RESOLV_CONF_TMP

    #if ble connect time great than 1 times, use predefined dns list
    local connect_times=0;
    mkdir -p /data/status/
    local CONNECT_TIMES_FILE="/data/status/mico_try_register_times"
    local country=$(micocfg_country)
    country=${country:-CN}
    local connect_times=$(cat $CONNECT_TIMES_FILE)
    connect_times=${connect_times:-0}

    mico_log "country $country connect times $connect_times"
    [ "$country" == "CN" ] && {
        [ $connect_times -gt 1 -o -f /data/status/use_predef_dns ] && {
            setup_predef_dns
            return
        }
    }

    [ -n "$domain" ] && {
        mico_log "domain $domain"
        echo search $domain >> $RESOLV_CONF_TMP
    }

    mico_log "dns list $dns"
    for i in $dns ; do
        echo nameserver $i >> $RESOLV_CONF_TMP
    done

    cp $RESOLV_CONF_TMP $RESOLV_CONF -f >> /dev/null
}

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
    setup_dns
}

deconfig_interface() {
    /sbin/ifconfig $interface 0.0.0.0
}

#urgly fix for delay show light 6 at init issue if no ip bounded 
delay_show_light()
{
    [ -f /tmp/dhcp_dlay_showing ] && {
        return;
    }

    touch /tmp/dhcp_dlay_showing
    sleep 10
    [ -f /tmp/dhcp_done_flag ] && {   
        mico_log "[dhcp delay led] dhcp done, not show led 6"     
        return
    }
    mico_log "[dhcp delay led] show led 6"
    /bin/show_led 6
    rm  /tmp/dhcp_dlay_showing
}

show_light()
{
    [  ! -f "/data/status/config_done" ] && {
        return
    }

    [ -f /data/status/work_mode ] && {
        work_mode=`mode_switch get_work_mode`
        [ "bluetooth" = "$work_mode" ] && {
            mico_log "[dhcp led] work_mode is bluetooth, not show led"
            return
        }
    }

    [ -f /tmp/hibernate_mode ] && {
        return
    }

    [ -f /tmp/power_off ] && {
        return
    }

    [ -f /tmp/pseudo_power_off ] && {
        return
    }

    [ -f /tmp/wifi_check_ccmp_err ] && {
        return
    }
    
    [ ! -f /tmp/simple_dhcp_bounded ] && {
        delay_show_light &
        return
    }

    mico_log "[dhcp led] show led 6"
    /bin/show_led 6
    
}

miio_wireless_connected_event_old() {
    [ `micocfg_miot_auto_provision_support` != "yes" ] && {
        mico_log "no need send wireless_connected_event event"
        return;
    }

    #ssid=$(echo -en `wpa_cli -i wlan0 status|grep ^ssid=|cut -d = -f 2`|sed 's/\\"/"/g')
    #bssid=$(wpa_cli -i wlan0 status | grep ^bssid | cut -d= -f 2)
    ssid=`micocfg_ssid`
    bssid=`micocfg_bssid`

    echo -n "$ssid" > /tmp/wifi_connected_info.ssid
    echo -n "$bssid" > /tmp/wifi_connected_info.bssid

    # 避免出现使用音箱app配网时触发wifi_connected导致miio提前进入绑定流程
    REGISTER_FLAG="/tmp/mico_register_is_working"
    [ -f $REGISTER_FLAG ] && {
        mico_log "#LOCK# already locked by $(cat $REGISTER_FLAG)"
        [ -f $REGISTER_FLAG -a "miio" != "$(cat $REGISTER_FLAG)" ] && {
            return;
        }
    }

    mico_log "ubus call miio wifi_connected  {\"from\":\"$LOG_TITLE\",\"ssid\":\""$ssid"\",\"bssid\":\"$bssid\"}"

    #ubus call miio wifi_connected  {\"from\":\"$LOG_TITLE\",\"ssid\":\""$ssid"\",\"bssid\":\"$bssid\"} &
    micocfg_miio_wifi_connected  "$LOG_TITLE" "$ssid" "$bssid"  &
}

miio_wireless_connected_event() {
    mico_log "ubus call miio dhcp {\"interface\":\"$interface\", \"type\":\"$1\"}"
    ubus call miio dhcp "{\"interface\":\"$interface\", \"type\":\"$1\"}" &
}

case "$1" in
    deconfig)
        show_light
        mico_log "[deconfig dhcp] release ip"
        deconfig_interface
        mico_log "rm /tmp/dhcp_done_flag"
        rm "/tmp/dhcp_done_flag" >/dev/null 2>&1
        miio_wireless_connected_event "release"
    ;;
    renew)
        setup_interface
        mico_log "[renew dhcp]"
        uptime_sec=$(awk '{printf("%d",$1)}' < /proc/uptime)
        echo "$interface $ip $router $uptime_sec $lease" > /tmp/dhcp_done_flag
        miio_wireless_connected_event "renew"
    ;;
    bound)
        touch /tmp/simple_dhcp_bounded
        setup_interface
        mico_log "[dhcp get ip success.]"
        #/etc/init.d/xiaomi_dns_server restart 1>/dev/null 2>&1
        /etc/init.d/dnsmasq  restart 1>/dev/null 2>&1
        mkdir -p /data/status/

        [ -f /tmp/dhcp_done_flag ] && logger stat_points_none spk_wifi_reconnect=1
        [ ! -f /tmp/dhcp_done_flag ] && logger stat_points_none spk_wifi_connect=1
        uptime_sec=$(awk '{printf("%d",$1)}' < /proc/uptime)
        echo "$interface $ip $router $uptime_sec $lease" > /tmp/dhcp_done_flag
        miio_wireless_connected_event "bound"
        #killall -9 ntpsetclock
        #/bin/ntpsetclock loop &
        /usr/sbin/network_probe.sh 1>/dev/null 2>&1 &
        mico_log "[dhcp restart services, services:[dlna,mitv-disc,alarm,mdplay,miplay,idmruntime,central_lite]]"
        if [ -f /etc/init.d/mdnsd ]; then
            mico_log "network change, flush mdnsd."
            /etc/init.d/mdnsd restart 1>/dev/null 2>&1
        fi
        /etc/init.d/dlnainit restart 1>/dev/null 2>&1
        /etc/init.d/mitv-disc  restart 1>/dev/null 2>&1
        /etc/init.d/alarm restart 1>/dev/null 2>&1
        hardware=$(micocfg_model)
        if [ "M03A" != $hardware ]; then
            /etc/init.d/mico_aivs_lab restart 1>/dev/null 2>&1
        fi
        if [ -f /etc/init.d/mdplay ] && [ $(pidof mdplay) ]; then
            mico_log "mdplay running, but network change, need restart."
            /etc/init.d/mdplay restart 1>/dev/null 2>&1
        fi
        if [ -f /etc/init.d/miplay ]; then
            mico_log "miplay running, but network change, need restart."
            /etc/init.d/miplay restart 1>/dev/null 2>&1
        fi
        if [ -f /etc/init.d/airplay ]; then
            mico_log "network change, flush airplay."
            ubus -t 1 call airplay update_bonjour &
        fi

        [ -f /data/status/config_done ] && {
            [ `micocfg_miot_auto_provision_support` != "yes" ] && {
                /etc/init.d/miio restart
            }
        }

        register_type=$(cat /data/status/register_type)

        #if cmcc andlink not enable, this line will not work
        #/etc/init.d/cmcc_andlink restart
        #only ble type not start cmcc, 
        #if value empty or empty, start cmcc.
        [ x"$register_type" != x"mico" ] && {
            mico_log "[dhcp try re register cmcc ims ]"
            /usr/bin/cmcc_helper -a register &
        }

        # restart idmruntime, as mdns need reset when ip change.
        /etc/init.d/idmruntime restart 1>/dev/null 2>&1
        if [ -f /etc/init.d/central_lite ]; then
            if [ "$(micocfg_get /data/bt/bluetooth.cfg btmesh_enable)" -eq 1 ]; then
                /etc/init.d/central_lite restart >/dev/null 2>&1
            fi
        fi
        mico_log "[dhcp restart service success ]"
#       /etc/init.d/messagingagent restart 1>/dev/null 2>&1
        if [ "x`micocfg_ssid`" != "x$MIOT_HIDDEN_SSID" ]; then {
            if [ ! -f /tmp/wifi_check_ccmp_err ]; then {
                mico_log "[dhcp led] shut led 6"
                [ -f /data/status/config_done ] && {
                    /bin/shut_led 6
                }
            }
            else {
                mico_log "[dhcp led] remove wifi_check_ccmp_err"
                rm -f /tmp/wifi_check_ccmp_err
                [ -f /data/status/config_done ] && {
                    /bin/shut_led 6
                }
            }
            fi
        }
        else {
            # for auto_provision
            mico_log "[dhcp led] not shut led 6: runing auto provision"
        }
        fi
        /usr/bin/upload_kernel_crash.sh &
    ;;
    setup_predef_dns)
        setup_predef_dns
    ;;
esac
