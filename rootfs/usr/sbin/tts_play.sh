source /usr/share/libubox/jshn.sh

play_text()
{
    local _result=$(ubus call mibrain text_to_speech "{\"text\":\"$1\",\"save\":1}")
    [ x"$_result" == x"" ] && exit 1
    json_init 
    json_load "$_result"
    json_get_var _info info  
    json_cleanup
    echo $_info 
    json_init 
    json_load "$_info"
    json_get_var _path path  
    json_cleanup
    my_log "play text tts created $_path"
    echo $_path
    #[ x"$_path" == x"" ] && {
    #    miplayer -f /usr/share/sound/mibrain_service_unavailable.opus
    #    return 1
    #}
    miplayer -f $_path 
    rm $_path
    return 0
}

player_stat=`/usr/bin/mphelper mute_stat`
/usr/bin/mphelper pause
play_text "$1"
[ x"$player_stat" == x"1" ] && {
    /usr/bin/mphelper play
}
