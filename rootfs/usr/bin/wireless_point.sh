#!/bin/sh


LOG_TITLE="wireless_point"
RATE_STAT=0
RATE_CNT=0
RATE_MEAN=0
ROM_TYPE=$(micocfg_model)
#BOARD_TYPE==4 : s12c
BOARD_TYPE=`fw_env -g board_id`
[ "$BOARD_TYPE" == "4" ] && {
    ROM_TYPE="s12c"
}
ROM_TYPE=$(echo $ROM_TYPE|tr '[A-Z]' '[a-z]')


mico_log() {
    logger -t $LOG_TITLE[$$][REGISTER] -p 3 "$*"
    echo $* 
}

rate_stat_timeout() 
{
    local current_time=$(date +%s)
    
    [ $current_time -ge $RATE_STAT_END_TIME ] && { 
        mico_log "rate stat timeout current_time $current_time end $RATE_STAT_END_TIME"
        mico_log "rate stat $RATE_STAT count $RATE_CNT"
        RATE_MEAN=0
        [ $RATE_CNT -ne 0 ] && {
            RATE_MEAN=$(($RATE_STAT / $RATE_CNT))
        }
        mico_log "rate stat mean $RATE_MEAN"
        logger stat_points_none wifi_mean_rate=$RATE_MEAN
        RATE_STAT_END_TIME=$(($(date +%s) + $RATE_STAT_TIMEOUT_INT))
        RATE_STAT=0
        RATE_CNT=0
        mico_log "After print rate stat timeout current_time $current_time end $RATE_STAT_END_TIME"
        mico_log "After print rate stat $RATE_STAT count $RATE_CNT mean $RATE_MEAN"
    };

    return 0;
}


mico_log "wireless point start."

RATE_STAT_TIMEOUT_INT=18000
RATE_STAT_END_TIME=$(($(date +%s) + $RATE_STAT_TIMEOUT_INT))
rate=0
bssid=""

while true
do
    rate_stat_timeout
    bssid=`wpa_cli status |grep bssid`
    [ "$bssid" == "" ] && {
        RATE_CNT=$(($RATE_CNT + 1))
        sleep 1
        continue;
    }
    
    case $ROM_TYPE in
    S12|s12|S12A|s12a)
        rate=$(wl rate | awk  '{print $1}')

    ;;
    LX01|lx01)
        rate=$(wl rate | awk  '{print $1}')

    ;;
    M01|m01)
        rate=$(wl rate | awk  '{print $1}')

    ;;
    s12c)
        rate=$(mlanutl wlan0 getdatarate  | grep Rate | sed -n '2p' | awk '{print $2}' | awk -F '.' '{print $1}')
    ;;
    LX05|lx05)

    ;;
    esac

    RATE_CNT=$(($RATE_CNT + 1))
    RATE_STAT=$(($RATE_STAT + $rate))
    sleep 10s
done


