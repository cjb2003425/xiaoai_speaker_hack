#include <iostream>
#include <string>
#include <thread>
#include <peer.h>
#include <sys/inotify.h>
#include <fstream> 
#include <unistd.h>
#include <signal.h>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp> 
#include "ThreadTimer.hpp"
extern "C" {
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include "libubox/blob.h"
}
#include "main.h"

using json = nlohmann::json;
using namespace std;

const string env_config_path = "/data/env_config";
string instruction_file_name = "/tmp/mico_aivs_lab/instruction.log";
bool wakeup_occurred = false;
bool wakeup_first = true;
mutex wakeup_mtx;
condition_variable wakeup_cond;

// Unordered map to store unique items by "id"
unordered_map<string, json> items;
string current_dialog_id;
static struct ubus_context *ctx;
struct blob_buf b;

static int monitor_dir = 1;
static uint32_t monitor_mask = 0x20; //invoke message
static const char * const monitor_types[] = {
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

int store(json& data) {
    string text;
    string id = data["header"]["id"];
    
    if (items.find(id) == items.end()) {
        items[id] = data;
        if (data["header"]["name"] == "RecognizeResult" && data["payload"]["is_final"] == true) {
            text = data["payload"]["results"][0]["text"];
            cout << "final result:" << text << endl;
            create_conversation_item(text);
            create_response();
        }
        cout << data["header"]["name"] << endl;
    }

    return 0;
}

void parseFileContent(const std::string& file_name) {
    string dialog_id;
    ifstream file(file_name);
    if (!file.is_open()) {
        cerr << "Could not open file: " << file_name << std::endl;
        return;
    }

    //cerr << file_name << "was been modidified" << endl;
    try {
        // Read the file line by line to handle multiple JSON objects
        std::string line;
        while (std::getline(file, line)) {
            // Parse each line as a JSON object
            json json_data = json::parse(line);
            dialog_id = json_data["header"]["dialog_id"];

            if (dialog_id != current_dialog_id) {
                //new dialog, clear saved data
                cout << "clear..." << endl;
                items.clear();
                current_dialog_id = dialog_id;
            }
            store(json_data);
        }
    } catch (json::parse_error& e) {
        //ignore
        cerr << "JSON parse error: " << e.what() << std::endl;
    }
}

void monitorFileChanges() {
    unique_lock<mutex> lock(wakeup_mtx);
    wakeup_cond.wait(lock, []{ return wakeup_occurred; });

    int fd = inotify_init();
    if (fd == -1) {
        std::cerr << "Failed to initialize inotify!" << std::endl;
        return;
    }

    int wd = inotify_add_watch(fd, instruction_file_name.c_str(), IN_MODIFY);
    if (wd == -1) {
        std::cerr << "Failed to add watch on file: " << instruction_file_name << std::endl;
        close(fd);
        return;
    }

    char buffer[1024];
    while (true) {
        int length = read(fd, buffer, sizeof(buffer));
        if (length == -1) {
            cerr << "Error reading events!" << std::endl;
            close(fd);
            return;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            if (event->mask & IN_MODIFY) {
                // Get the file name associated with the watch descriptor
                parseFileContent(instruction_file_name);
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    // Clean up
    inotify_rm_watch(fd, wd);
    close(fd);
}

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
    if (wakeup_first && json_data["method"] == "player_wakeup") {
        lock_guard<mutex> lock(wakeup_mtx);
        wakeup_occurred = true; 
        wakeup_cond.notify_all();
        wakeup_first = false;
    }
    //printf("%s %08x #%08x %14s: %s\n", send ? "->" : "<-", client, peer, ubus_msg_type(type), data);
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

void handle_siginit(int sig) {
    cout << "handle sigint" << endl;
    exit(1);
}

void loadEnvConfig(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error opening config file" << std::endl;
        exit(1);
    }

    std::string line;
    
    // Read each line of the config file
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the position of the '=' character to split key and value
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);

            // Remove any leading/trailing whitespaces
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // Export the environment variable
            if (setenv(key.c_str(), value.c_str(), 1) != 0) {
                std::cerr << "Error setting environment variable" << std::endl;
                exit(1);
            }
            std::cout << "Exported: " << key << "=" << value << std::endl;
        }
    }

    file.close();
}

int main(void) {
    signal(SIGINT, handle_siginit); 
    loadEnvConfig(env_config_path);
    std::thread file_monitor(monitorFileChanges);
    std::thread ubus_monitor(ubus_monitor_fun);
    ThreadTimer timer;

    pthread_t thread = pthread_self();
    struct sched_param param;
    param.sched_priority = 50;

    int ret = pthread_setschedparam(thread, SCHED_FIFO, &param);
    if (ret != 0) {
        perror("pthread_setschedparam");
        return -1;
    }
    oai_init_audio_capture();
    oai_init_audio_decoder();

    while (true) {
        oai_webrtc(timer);
    }
    file_monitor.join();
    ubus_monitor.join();
    return 0;
}
