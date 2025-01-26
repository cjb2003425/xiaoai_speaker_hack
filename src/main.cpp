#include <iostream>
#include <string>
#include <thread>
#include <peer.h>
#include <sys/inotify.h>
#include <fstream> 
#include <unistd.h>
#include <signal.h>
#include <nlohmann/json.hpp> 
extern "C" {
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
}
#include "main.h"

using json = nlohmann::json;
using namespace std;
vector<string> fileNames = {
  "/tmp/mico_aivs_lab/instruction.log",
  "/tmp/mico_aivs_lab/event.log"
};

// Unordered map to store unique items by "id"
unordered_map<string, json> items;
string current_dialog_id;
static struct ubus_context *ctx;
struct blob_buf b;

int mute();

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
            mute();
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
            if (file_name == fileNames[0]) {
              //instruction
              dialog_id = json_data["header"]["dialog_id"];
            } else if (file_name == fileNames[1]) {
              //event
              dialog_id = json_data["header"]["id"];
            } else {
              cerr << "Wront file:" << file_name << endl;
            }

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
    int fd = inotify_init();
    if (fd == -1) {
        std::cerr << "Failed to initialize inotify!" << std::endl;
        return;
    }

    std::unordered_map<int, std::string> watchMap; // Map to keep track of watch descriptors
    for (const auto& fileName : fileNames) {
        int wd = inotify_add_watch(fd, fileName.c_str(), IN_MODIFY);
        if (wd == -1) {
            std::cerr << "Failed to add watch on file: " << fileName << std::endl;
            close(fd);
            return;
        }
        watchMap[wd] = fileName; // Store the mapping of watch descriptor to file name
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
                std::string modifiedFile = watchMap[event->wd];
                parseFileContent(modifiedFile);
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    // Clean up
    for (const auto& pair : watchMap) {
        inotify_rm_watch(fd, pair.first);
    }
    close(fd);
}

int ubus_init() {
    // Initialize ubus context
    ctx = ubus_connect(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to connect to ubus\n");
        return -1;
    }
}

int ubus_deinit() {
    // Clean up
    ubus_free(ctx);  // Disconnect ubus
}


int mute () {
    uint32_t id;
    const char *ubus_object_path = "mediaplayer";
    const char *message = "{\"action\":\"stop\"}";

    blob_buf_init(&b, 0);
    if (!blobmsg_add_json_from_string(&b, message)) {
        cerr << "Fail to parse message data" << endl;
        return -1;
    }

    int ret = ubus_lookup_id(ctx, ubus_object_path, &id);
    if (ret) {
        fprintf(stderr, "Failed to lookup ubus object: %s\n", ubus_object_path);
        ubus_free(ctx);
        return -1;
    }
    
    // Now invoke the command using the stored object ID
    ret = ubus_invoke(ctx, id, "player_wakeup", b.head, NULL, NULL, 0);
    if (ret) {
        std::cerr << "Failed to invoke ubus command" << std::endl;
        blob_buf_free(&b);          // Free the blob buffer
        return -1;
    }
}

void handle_siginit(int sig) {
    cout << "handle sigint" << endl;
    exit(1);
}

int main(void) {
    signal(SIGINT, handle_siginit); 
    ubus_init();
    std::thread monitor(monitorFileChanges);
    peer_init();
    oai_init_audio_capture();
    oai_init_audio_decoder();
    oai_webrtc();

}
