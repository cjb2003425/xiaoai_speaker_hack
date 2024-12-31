#!/bin/sh /etc/rc.common

mico_log()
{
    echo "/etc/init.d/boot $*"
    logger -t /etc/init.d/boot -p 3 "$*"
}

binfo_translate_to_config()
{
    mkdir -p /data/status/
    #miio 2020 will remove /data/status @init issue
    flag_file_old="/data/status/uci_translated_flag"
    flag_file="/data/etc/uci_translated_flag"
    dev_info_check
    [ x"$?" == x"1" ] && {
        rm $flag_file $flag_file_old
    }
    [ -e $flag_file -o -e $flag_file_old ] && return;
    touch $flag_file
    fsync $flag_file

    [  -f "/data/etc/binfo"  ] && {
        cat /data/etc/binfo |awk -F "[ \']" '{if($1~/option/) printf tolower($2)" = \""$4"\";\n"}' > /data/etc/device.info
        fsync /data/etc/device.info
        sed -i '1i\#这个文件已经废弃，新文件位置 /data/etc/device.info' /data/etc/binfo
    }

    [ -f "/data/messagingagent/messaging" ] && {
        cat /data/messagingagent/messaging |awk -F "[ \']" '{if($1~/option/) printf tolower($2)" = \""$4"\";\n"}' > /data/etc/messaging.cfg
        fsync /data/etc/messaging.cfg
        sed -i '1i\#这个文件已经废弃，新文件位置 /data/etc/messaging.cfg' /data/messagingagent/messaging
    }
}

binfo_fix_board_info()
{
    local cfg_board_id=$(micocfg_board_id)
    [ x"$cfg_board_id" != x"" ] && return

uci -c /data/etc -q batch <<EOF
    set binfo.binfo.board_id=$board_id
    set binfo.binfo.board_name=$board_name
    set binfo.binfo.wifi_chip=$wifi_chip
    commit binfo
EOF
    micocfg_set /data/etc/device.info board_id "$board_id"
    micocfg_set /data/etc/device.info board_name "$board_name"
    micocfg_set /data/etc/device.info wifi_chip "$wifi_chip"

}

#console_login_prepare()
#{
#    /bin/mi_console
#}


uci_apply_defaults() 
{
    . /lib/functions/system.sh

    cd /etc/uci-defaults || return 0
    files="$(ls)"
    [ -z "$files" ] && return 0
    mkdir -p /tmp/.uci
    for file in $files; do
        ( . "./$(basename $file)" ) && rm -f "$file"
    done
    uci commit
}

binfo_check()
{
    [ ! -f /data/etc/binfo ] && return;

    for i in sn mac_wifi mac_bt miio_did miio_key
    do
        uci -c /data/etc get binfo.binfo.$i
        if [ $? != 0 ]; then
            rm /data/etc/binfo
            break
        fi
    done
}

dev_info_check()
{
    [ ! -f /data/etc/device.info ] && return 1;

    for i in sn mac_wifi mac_bt miio_did miio_key
    do
        micocfg_get /data/etc/device.info $i
        if [ $? != 0 ]; then
            rm /data/etc/device.info
            return 1
        fi
    done
    return 0
}

data_dir_prepare()
{
    local dir_list="/data/etc/ /data/wifi /data/bt /data/miio"
    for one_dir in $dir_list
    do
        mico_log "create dir $one_dir " 
        /bin/mkdir $one_dir
        /bin/chmod 777 $one_dir
    done
}

sound_vendor_prepare()
{
    if ! [ -e /data/sound ]; then
        tts_vendor=$(micocfg_tts_vendor)
        ln -s /usr/share/sound-vendor/${tts_vendor} /data/sound
    elif ! [ -e /data/mipns/tts_vendor ]; then
        link_file=`ls -l /data/sound`
        tts_vendor=${link_file##*/}
        echo -e "$tts_vendor\c" > /data/mipns/tts_vendor
    fi
}

try_cp() 
{
	[ ! -f $2 ] && cp $1 $2
}

bluetooth_recent_bd_fix()
{
    [ ! -f /data/bt/mibt_recent_data.json ] && return;
    . /usr/share/libubox/jshn.sh
    json_init
    json_load "$(cat /data/bt/mibt_recent_data.json)"
    json_get_var avk_recent_bd avk_recent_bd
    json_get_var av_recent_bd av_recent_bd
    json_cleanup

    echo  avk_recent_bd:$avk_recent_bd
    echo  av_recent_bd:$av_recent_bd
    [ "$avk_recent_bd" != "" ] && micocfg_set /data/bt/bluetooth.cfg avk_recent_bd $avk_recent_bd
    [ "$av_recent_bd" != "" ] && micocfg_set /data/bt/bluetooth.cfg av_recent_bd $av_recent_bd

    mv /data/bt/mibt_recent_data.json /data/bt/mibt_recent_data.json.old
}

bluetooth_init()
{
    [ ! -f /data/wifi/wpa_supplicant.conf -a ! -f /data/bt/bluetooth.cfg ] && {
        micocfg_bt_discoverable_set 1
        micocfg_bt_connectable_set 1
    }

    micocfg_bt_enable_set 1

    [ -f /data/bt/mibt_config.json ] && {
        . /usr/share/libubox/jshn.sh
        json_init
        json_load "$(cat /data/bt/mibt_config.json)"
        json_select device
        json_get_var enable enable
        json_get_var connectable connectable
        json_get_var discoverable discoverable
        json_cleanup
        micocfg_bt_discoverable_set  $discoverable
        micocfg_bt_connectable_set  $connectable
        micocfg_bt_enable_set $enable
        rm /data/bt/mibt_config.json
    } 
    
    [ -f /data/bt/mibt_mesh_config.json ] && {
        . /usr/share/libubox/jshn.sh
        json_init 1>/dev/null 2>/dev/null
        json_load "$(cat /data/bt/mibt_mesh_config.json)"  1>/dev/null 2>/dev/null
        json_get_var mesh_is_enable "mesh_is_enable" 1>/dev/null 2>/dev/null
        json_cleanup
        [ "$mesh_is_enable" == "1" ] && {
            micocfg_set /data/bt/bluetooth.cfg btmesh_enable 1
            rm /data/bt/mibt_mesh_config.json
        }
    }

    bluetooth_recent_bd_fix
}

#sys_status_prepare()
#{
#    [ -f /data/status/config_done ] && {
#        micocfg_sys_status_set success 
#        rm /data/status/config_done
#        return;
#    }

#    [ ! -f /data/etc/status.cfg ] &&　{
#        micocfg_sys_status_set init
#    }
#}

openwrt_env_prepare()
{
    [ -f /proc/mounts ] || /sbin/mount_root
    [ -f /proc/jffs2_bbc ] && echo "S" > /proc/jffs2_bbc
    [ -f /proc/net/vlan/config ] && vconfig set_name_type DEV_PLUS_VID_NO_PAD

    mkdir -p /var/run
    mkdir -p /var/log
    mkdir -p /var/lock
    mkdir -p /var/state
    mkdir -p /var/tmp
    mkdir -p /tmp/.uci
    chmod 0700 /tmp/.uci
    touch /var/log/wtmp
    touch /var/log/lastlog
    touch /tmp/resolv.conf.auto
    ln -sf /tmp/resolv.conf.auto /tmp/resolv.conf
    grep -q debugfs /proc/filesystems && /bin/mount -o noatime -t debugfs debugfs /sys/kernel/debug
    [ "$FAILSAFE" = "true" ] && touch /tmp/.failsafe
}

timezone_init()
{
    #default PRC
    zonename="PRC"
    [ -n "$zonename" ] && [ -f "/usr/share/zoneinfo/$zonename" ] && \
        ln -sf "/usr/share/zoneinfo/$zonename" /tmp/localtime && rm -f /tmp/TZ
    date -k
}

time_init_buildtime()
{
    timezone_init
    buildts=$(micocfg_build_time)
    [ "x$buildts" != "x" ] && date -s "$buildts"
}

hostname_set()
{
    local myname=$(micocfg_model);
    echo "${myname:-mico}" > /proc/sys/kernel/hostname
}

hostname_set_sn()
{
    local myname=$(micocfg_hostname);
    echo "${myname:-mico}" > /proc/sys/kernel/hostname
}

miio_fix_registed_file()
{
    local uid=$(micocfg_uid)
    [ x"$uid" == x"-1" -o x"$uid" == x"" ] && return
    micocfg_set /data/etc/miio.cfg registed true
}
