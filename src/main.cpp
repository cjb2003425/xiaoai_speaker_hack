#include <iostream>
#include <string>
#include <thread>
#include <peer.h>
#include <sys/inotify.h>
#include <fstream> 
#include <unistd.h>
#include <signal.h>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp> 
#include "ThreadTimer.hpp"
#include "ubus.h"
#include "main.h"
#include "utils.h"
#include "WebSocketClient.h"
#include "WebRTCClient.h"

using json = nlohmann::json;
using namespace std;

const string env_config_path = "/data/env_config";
string instruction_file_name = "/tmp/mico_aivs_lab/instruction.log";

// Unordered map to store unique items by "id"
unordered_map<string, json> items;
string current_dialog_id;
RealTimeClient *client = nullptr;

int store(json& data) {
    string text;
    string result;
    string id = data["header"]["id"];
    
    if (items.find(id) == items.end()) {
        items[id] = data;
        if (data["header"]["name"] == "RecognizeResult" && data["payload"]["is_final"] == true) {
            text = data["payload"]["results"][0]["text"];
            cout << "final result:" << text << endl;
            if (client && text.length() > 0) {
                client->createConversationitem(text);
                client->createResponse();
            }
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
    ubus_wait_first_wakeup();
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

RealTimeClient* oai_get_client(void) {
    return client;
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
    #ifndef USE_WEBRTC
    client = new WebRTCClient(timer);
    #else
    client = new WebSocketClient();
    #endif
    client->init();
    client->loop();
    file_monitor.join();
    ubus_monitor.join();
    return 0;
}
