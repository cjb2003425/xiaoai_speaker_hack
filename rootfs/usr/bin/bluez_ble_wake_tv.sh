#!/bin/sh

# inquiry local bluetooth device
#hcitool dev
#sudo hcitool -i hcix cmd <OGF> <OCF> <No. Significant Data Octets> <iBeacon Prefix> <UUID> <Major> <Minor> <Tx Power> <Placeholder Octets>
#OGF = Operation Group Field = Bluetooth Command Group = 0x08
#OCF = Operation Command Field = HCI_LE_Set_Advertising_Data = 0x0008

# set -x
export BLUETOOTH_DEVICE=hci0

function wake_tv_log()
{ 
    logger -t wake_tv[$$] -p 3 "$*"
    # echo "$*"
}


function wake_tv_start() 
{
    local_random_mac=`cat /dev/urandom | head -c 6 | hexdump -C | awk '{print $2, $3,$4, $5, $6, $7}' 2>/dev/null`
    tv_mac=`printf $1 | awk '{gsub(/:/," "); print $6, $5, $4, $3, $2, $1}'`

    wake_tv_log "=======================wake tv advertivsing start====================="

    # Disable advertising
    cmd_ret=`hcitool -i $BLUETOOTH_DEVICE cmd 0x08 0x000a 00`
    res=`echo -n $cmd_ret | awk 'END {print $0};' | grep -i "0a 20 00"`
    [ -z "$res" ] && { 
        wake_tv_log $cmd_ret
        exit 1
    }
    
    # Set random addcmd_rets
    cmd_ret=`hcitool -i $BLUETOOTH_DEVICE cmd 0x08 0x0005 $local_random_mac`
    res=`echo -n $cmd_ret | awk 'END {print $0}' | grep -i "05 20 00"`
    [ -z "$res" ] && { 
        wake_tv_log $cmd_ret
        exit 1
    }
    

    # Set advertising data
    cmd_ret=`hcitool -i $BLUETOOTH_DEVICE cmd 0x08 0x0008 15 11 07 00 01 02 01 05 03 ff 00 01 $tv_mac 00 02 01 1a 00 00 00 00 00 00 00 00 00 00`
    res=`echo -n $cmd_ret | awk 'END {print $0}' | grep -i "08 20 00"`
    [ -z "$res" ] && { 
        wake_tv_log $cmd_ret
        exit 1
    }
    
    # Set advertising parameter
    cmd_ret=`hcitool -i $BLUETOOTH_DEVICE cmd 0x08 0x0006 30 00 30 00 00 01 00 00 00 00 00 00 00 07 00`
    res=`echo -n $cmd_ret | awk 'END {print $0}' | grep -i "06 20 00"`
    [ -z "$res" ] && { 
        wake_tv_log $cmd_ret
        exit 1
    }
   

    # Enable advertising
    cmd_ret=`hcitool -i $BLUETOOTH_DEVICE cmd 0x08 0x000a 01`
    res=`echo -n $cmd_ret | awk 'END {print $0}' | grep -i "0a 20 00"`
    [ -z "$res" ] && { 
        wake_tv_log $cmd_ret
        exit 1
    }
    

    [ $2 ] && {
        (
            sleep $2;
            wake_tv_stop;
        ) &
    }
    return 0
}

function wake_tv_stop() 
{
    cmd_ret=`hcitool -i $BLUETOOTH_DEVICE cmd 0x08 0x000a 00`
    res=`echo -n $cmd_ret | awk 'END {print $0}' | grep -i "0a 20 00"`
    [ -z "$res" ] && { 
        wake_tv_log $cmd_ret
        # hciconfig hci0 reset
        exit 1
    }

    wake_tv_log "=======================wake tv advertivsing stop======================"
    return 0
}

case $1 in
    start)
        wake_tv_start $2 $3
        ;;
    stop)
        wake_tv_stop
        ;;
    *)
        wake_tv_log "wake tv param error"
        ;;
esac