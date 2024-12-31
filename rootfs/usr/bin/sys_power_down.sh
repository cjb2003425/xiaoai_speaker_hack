#!/bin/sh

is_tts_playing()
{
    play=`ubus -t 1 call  mediaplayer player_get_play_status | grep info | grep '\\\\"status\\\\": 1'`
    if [ "$play" != "" ]; then
            echo playing
            media=`echo $play | grep media_type`
            if [ "$media" != "" ]; then
                    echo mediaplay
                    mphelper pause
                    return 0
            fi
            return 1
    fi
    return 0
}

wait_tts_over()
{
    while :
    do
        is_tts_playing
        if [ $? = 0 ]; then
            break
        fi
        sleep 1
    done
}

model=`micocfg_model`
is_power_on()
{
    if [ "$model" = "M03A" ]; then
        powerdc=`cat /sys/class/power_supply/rt9467/online`
    elif [ "$model" = "L09B" ]; then
        powerdc=`cat /sys/devices/i2c-3/3-006b/vbus_status`
    fi
    if [ "$powerdc" = "0" ]; then
        return 0
    fi
    return 1
}

if [ "$model" = "L09B" -o "$model" = "M03A" ]; then
    if [ "$model" = "L09B" ]; then
        is_power_on
        if [ $? = 1 ]; then
            echo dc on now
            exit 0
        fi
    elif [ "$model" = "M03A" ]; then
        upgrade_status=`cat /data/status/ota`
        if [ "$upgrade_status" = "burn" ]; then
            logger -t sys_power_down.sh -p 3 "upgrade status is burn,no power off"
            exit 0
        fi
        if [ -f /tmp/pseudo_power_off ] ; then
            logger -t sys_power_down.sh -p 3 "pseudo power off,pseudo"
            /usr/bin/mode_switch power_off
            exit 0
        fi
    fi
    echo waiting 2s
    logger -t sys_power_down.sh -p 3 "power off waiting 2s"
    sleep 2
    echo easy_logcut
    /usr/sbin/easy_logcut
    echo wait_tts_over
    wait_tts_over
    is_power_on
    if [ $? = 0 ]; then
        echo power off now
        shut_led 27
        shut_led 35
        qplayer /usr/share/common_sound/power_off.opus >/dev/null 2>&1
        logger -t sys_power_down.sh -p 3 "power off"
        if [ "$model" = "M03A" ]; then
            echo 1 > /tmp/power_off
            poweroff
        elif [ "$model" = "L09B" ]; then
            echo 0xff634458 0xfffffffe > /sys/kernel/debug/aml_reg/paddr
        fi
    else
        if [ "$model" = "M03A" ]; then
            logger -t sys_power_down.sh -p 3 "pseudo power off,dc on"
            /usr/bin/mode_switch power_off
        elif [ "$model" = "L09B" ]; then
            logger -t sys_power_down.sh -p 3 "no power off,dc on"
        fi
        echo dc on now
    fi
fi
