#!/bin/sh
mico_log()
{
    logger -t "MICOREGISTER"$0 -p 3 "$*"
}

mico_log "rm old config"
rm /data/status -r -f > /dev/null 2>&1;
rm /data/messagingagent -r -f > /dev/null 2>&1;
rm /data/.mediaplayerconfig -r -f> /dev/null 2>&1;
rm /data/miio -r -f > /dev/null 2>&1;
rm /data/mibrain/mibrain_asr_nlp.rcd > /dev/null 2>&1;
rm /data/bt/bt_devices.xml > /dev/null 2>&1;
rm /data/bt/bt_av_devices.xml > /dev/null 2>&1;
rm /data/upnp-disc -r -f > /dev/null 2>&1;
/etc/init.d/alarm restart;
/etc/init.d/mediaplayer restart;
/etc/init.d/messagingagent restart;
/etc/init.d/miio restart;
/etc/init.d/pns restart
/etc/init.d/mibrain_service restart
