#ifndef UBUS_H
#define UBUS_H

#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include "libubox/blob.h"
#include <nlohmann/json.hpp>

void ubus_monitor_fun();
void ubus_wait_first_wakeup(); 
#endif
