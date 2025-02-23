#include <iostream>
#include <unistd.h>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
extern "C" {
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
}
#include "ubus.h"
#include "main.h"

using json = nlohmann::json;
using namespace std;

const char * const monitor_types[] = {
    [UBUS_MSG_HELLO] = "hello",
    [UBUS_MSG_STATUS] = "status",
    [UBUS_MSG_DATA] = "data",
    [UBUS_MSG_PING] = "ping",
    [UBUS_MSG_LOOKUP] = "lookup",
    [UBUS_MSG_INVOKE] = "invoke",
    [UBUS_MSG_ADD_OBJECT] = "add_object",
    [UBUS_MSG_REMOVE_OBJECT] = "remove_object",
    [UBUS_MSG_SUBSCRIBE] = "subscribe",
    [UBUS_MSG_UNSUBSCRIBE] = "unsubscribe",
    [UBUS_MSG_NOTIFY] = "notify",
};

static int monitor_dir = 1;
static uint32_t monitor_mask = 0x20; //invoke message
bool wakeup_first = true;
bool wakeup_occurred = false;
mutex wakeup_mtx;
condition_variable wakeup_cond;

static struct ubus_context *ctx;
struct blob_buf b;

static const char* ubus_msg_type(uint32_t type)
{
    const char *ret = NULL;
    static char unk_type[16];
    
    if (type < ARRAY_SIZE(monitor_types))
        ret = monitor_types[type];
    
    if (!ret) {
        snprintf(unk_type, sizeof(unk_type), "%d", type);
        ret = unk_type;
    }
    
    return ret;
}

static char* ubus_get_monitor_data(struct blob_attr *data)
{
    static const struct blob_attr_info policy[UBUS_ATTR_MAX] = {
        { BLOB_ATTR_UNSPEC, 0, 0, nullptr },
        { BLOB_ATTR_INT32, 0, 0, nullptr },  // UBUS_ATTR_STATUS
        { BLOB_ATTR_STRING, 0, 0, nullptr }, // UBUS_ATTR_OBJPATH
        { BLOB_ATTR_INT32, 0, 0, nullptr },  // UBUS_ATTR_OBJID
        { BLOB_ATTR_STRING, 0, 0, nullptr }, // UBUS_ATTR_METHOD
        { BLOB_ATTR_INT32, 0, 0, nullptr },  // UBUS_ATTR_OBJTYPE
        { BLOB_ATTR_NESTED, 0, 0, nullptr }, // UBUS_ATTR_SIGNATURE
        { BLOB_ATTR_NESTED, 0, 0, nullptr }, // UBUS_ATTR_DATA
        { BLOB_ATTR_UNSPEC, 0, 0, nullptr },
        { BLOB_ATTR_INT8, 0, 0, nullptr },   // UBUS_ATTR_ACTIVE
        { BLOB_ATTR_INT8, 0, 0, nullptr },   // UBUS_ATTR_NO_REPLY
        { BLOB_ATTR_UNSPEC, 0, 0, nullptr },
        { BLOB_ATTR_STRING, 0, 0, nullptr }, // UBUS_ATTR_USER
        { BLOB_ATTR_STRING, 0, 0, nullptr }  // UBUS_ATTR_GROUP
    };
    
    static const char * const names[UBUS_ATTR_MAX] = {
        nullptr,
        "status",    // UBUS_ATTR_STATUS
        "objpath",   // UBUS_ATTR_OBJPATH
        "objid",     // UBUS_ATTR_OBJID
        "method",    // UBUS_ATTR_METHOD
        "objtype",   // UBUS_ATTR_OBJTYPE
        "signature", // UBUS_ATTR_SIGNATURE
        "data",      // UBUS_ATTR_DATA
        nullptr,
        "active",    // UBUS_ATTR_ACTIVE
        "no_reply",  // UBUS_ATTR_NO_REPLY
        nullptr,
        "user",      // UBUS_ATTR_USER
        "group"      // UBUS_ATTR_GROUP
    };

    struct blob_attr *tb[UBUS_ATTR_MAX];
    int i;

    blob_buf_init(&b, 0);
    blob_parse(data, tb, policy, UBUS_ATTR_MAX);

    for (i = 0; i < UBUS_ATTR_MAX; i++) {
        const char *n = names[i];
        struct blob_attr *v = tb[i];

        if (!tb[i] || !n)
            continue;

        switch(policy[i].type) {
        case BLOB_ATTR_INT32:
            blobmsg_add_u32(&b, n, blob_get_int32(v));
            break;
        case BLOB_ATTR_STRING:
            blobmsg_add_string(&b, n, static_cast<const char*>(blob_data(v)));
            break;
        case BLOB_ATTR_INT8:
            blobmsg_add_u8(&b, n, !!blob_get_int8(v));
            break;
        case BLOB_ATTR_NESTED:
            blobmsg_add_field(&b, BLOBMSG_TYPE_TABLE, n, blobmsg_data(v), blobmsg_data_len(v));
            break;
        }
    }

    return blobmsg_format_json(b.head, true);
}

static void ubus_monitor_cb(struct ubus_context *ctx, uint32_t seq, struct blob_attr *msg)
{
    static const struct blob_attr_info policy[UBUS_MONITOR_MAX] = {
        { BLOB_ATTR_INT32, 0, 0, nullptr }, // UBUS_MONITOR_CLIENT
        { BLOB_ATTR_INT32, 0, 0, nullptr }, // UBUS_MONITOR_PEER
        { BLOB_ATTR_INT8, 0, 0, nullptr },  // UBUS_MONITOR_SEND
        { BLOB_ATTR_INT32, 0, 0, nullptr }, // UBUS_MONITOR_TYPE
        { BLOB_ATTR_NESTED, 0, 0, nullptr } // UBUS_MONITOR_DATA
        };

    struct blob_attr *tb[UBUS_MONITOR_MAX];
    uint32_t client, peer, type;
    bool send;
    char *data;
    
    blob_parse(msg, tb, policy, UBUS_MONITOR_MAX);
    
    if (!tb[UBUS_MONITOR_CLIENT] ||
        !tb[UBUS_MONITOR_PEER] ||
        !tb[UBUS_MONITOR_SEND] ||
        !tb[UBUS_MONITOR_TYPE] ||
        !tb[UBUS_MONITOR_DATA]) {
        printf("Invalid monitor msg\n");
        return;
    }
    
    send = blob_get_int32(tb[UBUS_MONITOR_SEND]);
    client = blob_get_int32(tb[UBUS_MONITOR_CLIENT]);
    peer = blob_get_int32(tb[UBUS_MONITOR_PEER]);
    type = blob_get_int32(tb[UBUS_MONITOR_TYPE]);
    
    if (monitor_mask && type < 32 && !(monitor_mask & (1 << type)))
        return;
    
    if (monitor_dir >= 0 && send != monitor_dir)
    return;
    
    data = ubus_get_monitor_data(tb[UBUS_MONITOR_DATA]);
    json json_data = json::parse(data);
    cout << "--> " << json_data["method"] << endl;
    if (json_data["data"]["action"] == "start" && json_data["method"] == "player_wakeup") {
        RealTimeClient* client = oai_get_client();
        if (client) {
            client->setWakeupOn(true);
        }

        if (wakeup_first) {
            lock_guard<mutex> lock(wakeup_mtx);
            wakeup_occurred = true; 
            wakeup_cond.notify_all();
            wakeup_first = false;
        }
    } else if (json_data["data"]["action"] == "stop" && json_data["method"] == "player_wakeup") {
        RealTimeClient* client = oai_get_client();
        if (client) {
            client->setWakeupOn(false);
        }
    }
    free(data);
    fflush(stdout);
}

void ubus_monitor_fun() {
    int ret;

    ctx = ubus_connect(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to connect to ubus\n");
        return;
    }

    uloop_init();
    ubus_add_uloop(ctx);
    ctx->monitor_cb = ubus_monitor_cb;
    ret = ubus_monitor_start(ctx);
    if (ret) {
        cerr << "ubus monitor start error" << endl;
        return;
    }

    uloop_run();
    uloop_done();

    ubus_monitor_stop(ctx);
}

void ubus_wait_first_wakeup() {
    unique_lock<mutex> lock(wakeup_mtx);
    wakeup_cond.wait(lock, []{ return wakeup_occurred; });
}

void ubus_exit() {
    lock_guard<mutex> lock(wakeup_mtx);
    wakeup_occurred = true; 
    wakeup_cond.notify_all();
    uloop_end();
}