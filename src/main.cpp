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
#include "CustomWebSocketClient.h" // Include the missing header
#include "HttpAPI.h"
#include <getopt.h>
#include <atomic>

using json = nlohmann::json;
using namespace std;

const string env_config_path = "/data/env_config";
string instruction_file_name = "/tmp/mico_aivs_lab/instruction.log";

// Unordered map to store unique items by "id"
unordered_map<string, json> items;
string current_dialog_id;
RealTimeClient *client = nullptr;

string host_address = "localhost";  // default value
string key_string = "";
static std::atomic<bool> monitoring_active{true};
bool custom_websocket_mode = false;

#if defined(__arm__)
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
        cerr << "[Dialog]JSON parse error: " << e.what() << std::endl;
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
    struct timeval timeout;
    fd_set fds;

    while (monitoring_active) {
        // Set up the file descriptor set
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        // Set timeout for select (100ms)
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        // Wait for events with timeout
        int ret = select(fd + 1, &fds, NULL, NULL, &timeout);
        
        if (ret < 0) {
            cerr << "Error in select!" << std::endl;
            break;
        }
        
        if (ret == 0) {
            // Timeout - check if we should continue
            continue;
        }

        if (FD_ISSET(fd, &fds)) {
            int length = read(fd, buffer, sizeof(buffer));
            if (length == -1) {
                cerr << "Error reading events!" << std::endl;
                break;
            }

            int i = 0;
            while (i < length) {
                struct inotify_event* event = (struct inotify_event*)&buffer[i];
                if (event->mask & IN_MODIFY) {
                    parseFileContent(instruction_file_name);
                }
                i += sizeof(struct inotify_event) + event->len;
            }
        }
    }
    std::cout << "Monitoring stopped" << std::endl;
    // Clean up
    inotify_rm_watch(fd, wd);
    close(fd);
}
#endif

void handle_siginit(int sig) {
    cout << "handle sigint" << endl;
    monitoring_active = false;
    #if defined(__arm__)
    ubus_exit();
    #endif
    client->quit();
}

void loadEnvConfig(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error opening config file" << std::endl;
        return;
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

void timerHandler(void) {
    std::string text = "猛犸象是如何灭绝的？";
    client->createConversationitem(text);
    client->createResponse();
}

std::string getConfigFromServer(std::string& url, std::string& key) {
    try {
        // Add https:// prefix if not present
        std::string full_url = url;
        if (full_url.find("http://") == std::string::npos && 
            full_url.find("https://") == std::string::npos) {
            full_url = "https://" + url + "/data";
        }
        
        HttpAPI api(full_url, key);

        const std::map<std::string, std::string> filters = {
            {"key", key}
        }; 
        std::string data = api.getData(filters);
        
        // Check if response contains HTML or error indicators
        if (data.find("<html>") != std::string::npos || 
            data.find("301") != std::string::npos) {
            std::cerr << "Received HTML response instead of expected data" << std::endl;
            std::cerr << data << std::endl;
            return "";
        }
        
        std::cout << "Data fetch successful: " << data << std::endl;
        return data;
    }
    catch (const std::exception& e) {
        std::cerr << "Application failed: " << e.what() << std::endl;
        return "";
    }
}

int main(int argc, char* argv[]) {
    uint32_t sample_rate = 0;
    bool websocket_mode = false;
    bool webrtc_mode = false;
    
    static struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"key", required_argument, 0, 'k'},
        {"websocket", no_argument, 0, 's'},
        {"webrtc", no_argument, 0, 'r'},
        {"custom", no_argument, 0, 'p'},  // Add new option
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "srph:k:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's':
                websocket_mode = true;
                break;
            case 'r':
                webrtc_mode = true;
                break;
            case 'p':
                custom_websocket_mode = true;
                break;
            case 'h':
                host_address = optarg;
                break;
            case 'k':
                key_string = optarg;
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " (-s | -r | -p) [--host <address>] [--key <key_string>]" << std::endl;
                std::cerr << "Options:" << std::endl;
                std::cerr << "  -s, --websocket       Use WebSocket client" << std::endl;
                std::cerr << "  -r, --webrtc         Use WebRTC client" << std::endl;
                std::cerr << "  -p, --custom         Use Custom WebSocket client" << std::endl;
                std::cerr << "  -h, --host <address> Specify host address (default: localhost)" << std::endl;
                std::cerr << "  -k, --key <string>   Specify key string" << std::endl;
                return 1;
        }
    }

    if (!websocket_mode && !webrtc_mode && !custom_websocket_mode) {
        std::cerr << "Error: Must specify either -s/--websocket, -r/--webrtc, or -p/--custom mode" << std::endl;
        return 1;
    }

    if ((websocket_mode && webrtc_mode) || 
        (websocket_mode && custom_websocket_mode) || 
        (webrtc_mode && custom_websocket_mode)) {
        std::cerr << "Error: Cannot specify multiple modes" << std::endl;
        return 1;
    }

    signal(SIGINT, handle_siginit);

    if (host_address.empty() || key_string.empty()) {
        std::cerr << "No host address or key string are provied" << std::endl;
        loadEnvConfig(env_config_path);
    } else {
        std::string data = getConfigFromServer(host_address, key_string);
        if (data.empty()) {
            std::cerr << "Failed to get config from server" << std::endl;
            loadEnvConfig(env_config_path);
        } else {
            std::cout << "Get config successfully" << std::endl;

            try {
                json config = json::parse(data);    
                std::string url = config["base_url"];
                std::string api_key = config["api_key"];
                std::string model = config["model"];
                std::string full_url = url + "?model=" + model;

                if (setenv("OPENAI_REALTIMEAPI", full_url.c_str(), 1) != 0) {
                    std::cerr << "Error setting environment variable" << std::endl;
                }

                if (setenv("OPENAI_API_KEY", api_key.c_str(), 1) != 0) {
                    std::cerr << "Error setting environment variable" << std::endl;
                }
                } catch (json::parse_error& e) {
                std::cerr << "JSON parsing error: " << e.what() << std::endl;
            }
        }
    }

    if (websocket_mode) {
        client = new WebSocketClient();
        sample_rate = 24000;
        oai_init_audio_alsa(sample_rate);
        oai_init_audio_decoder(sample_rate);
    } else if (custom_websocket_mode) {
        client = new CustomWebSocketClient();
    } else {  // webrtc_mode
        client = new WebRTCClient();
        sample_rate = 8000;
        oai_init_audio_alsa(sample_rate);
        oai_init_audio_decoder(sample_rate);
    }

    ThreadTimer timer;
    timer.set(3, timerHandler);
    timer.start();
    #if defined(__arm__)
    std::thread file_monitor(monitorFileChanges);
    std::thread ubus_monitor(ubus_monitor_fun);

    pthread_t thread = pthread_self();
    struct sched_param param;
    param.sched_priority = 50;

    int ret = pthread_setschedparam(thread, SCHED_FIFO, &param);
    if (ret != 0) {
        perror("pthread_setschedparam");
        return -1;
    }
    #endif

    client->init();
    client->loop();
    std::cout << "exit..." << std::endl;
    #if defined(__arm__)
    file_monitor.join();
    ubus_monitor.join();
    #endif
    return 1;
}
