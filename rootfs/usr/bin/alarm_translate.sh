#!/bin/sh

DBFILE="/data/.mediaplayerconfig/DataBase/alarm.db"
[ ! -f $DBFILE ] && exit;
#ID TIMESTAMP CIRCLE VOLUME STATUS EVENT UPDATE_TIMESTAMP REMINDER EXTRA BEHAVIOR BEHAVIOR_DATA RESERVE

DATA_DIR="/data/alarm/"
mkdir -p /data/alarm/
mkdir -p /data/timer/
rm /data/alarm/*
rm /data/timer/*

sqlite3 $DBFILE "select * from Alarm" |awk -F '\|' '{
     if(length($1)==0)
     {
         continue;
     };

     print "ID:"$1                >"/data/alarm/"$1;
     print "TIMESTAMP:"$2         >>"/data/alarm/"$1;
     print "CIRCLE:"$3            >>"/data/alarm/"$1;
     print "VOLUME:"$4            >>"/data/alarm/"$1;
     print "STATUS:"$5            >>"/data/alarm/"$1;
     print "EVENT:"$6             >>"/data/alarm/"$1;
     print "UPDATE_TIMESTAMP:"$7  >>"/data/alarm/"$1;
     print "REMINDER:"$8          >>"/data/alarm/"$1;
     print "CIRCLE_EXTRA:"$9      >>"/data/alarm/"$1;
     print "BEHAVIOR:"$10         >>"/data/alarm/"$1;
     print "BEHAVIOR_DATA:"$11    >>"/data/alarm/"$1;
     print "RESERVE:"$12          >>"/data/alarm/"$1;
}'

#ID TIMESTAMP EVENT VOLUME
sqlite3 $DBFILE "select * from Timer" |awk -F '\|' '{
     if(length($1)==0)
     {
         continue;
     };
     print "ID:"$1                >"/data/timer/"$1;
     print "TIMESTAMP:"$2         >>"/data/timer/"$1;
     print "EVENT:"$3             >>"/data/timer/"$1;
     print "VOLUME:"$4            >>"/data/timer/"$1;
}'

rm -rf /data/.mediaplayerconfig/DataBase
