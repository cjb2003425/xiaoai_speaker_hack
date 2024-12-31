#!/bin/sh

# for check network error

time=$(date +%H:%M:%S)

logger -s -p 3 -t "log_presence" "[$time] check presence: matool_get_presence"

res=$(matool_get_presence 2>/dev/null)
logger -s -p 3 -t "log_presence" "[$time] response:$res"


