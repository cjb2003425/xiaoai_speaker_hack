#! /bin/sh


/etc/init.d/pns stop
echo "recover" > /sys/kernel/debug/remoteproc/remoteproc0/recovery
echo "recover" > /sys/kernel/debug/remoteproc/remoteproc1/recovery
sleep 6
/etc/init.d/pns start
