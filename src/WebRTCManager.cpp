#include <stdio.h>
#include <time.h>
#include <chrono>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <atomic>
#include <string>
#include <nlohmann/json.hpp> 
#include <thread>
#include "main.h"
#include "WebRTCManager.h"
#include "ThreadTimer.hpp"

// For convenience
using json = nlohmann::json;

void WebRTCManager::onOpen(void* userdata) {
    WebRTCManager* manager = static_cast<WebRTCManager*>(userdata);
    char name[32] = "oai-event";
    char protocol[32] = "bar";
    printf("on open\n");
    if (peer_connection_create_datachannel(manager->peer_connection, DATA_CHANNEL_RELIABLE, 0, 0, name, protocol) == 13) {
       printf("data channel created\n"); 
    }
}

void WebRTCManager::onClose(void* userdata) {
    printf("on close\n");
}

void WebRTCManager::onMessage(char* msg, size_t len, void* userdata, uint16_t sid) {
    WebRTCManager* manager = static_cast<WebRTCManager*>(userdata);
    std::string json_data = std::string(msg, len);
    try {
        // Parse the JSON message
        json event = json::parse(json_data);
        std::cout << "message type is " << event["type"] << std::endl;
        if (event["type"] == "session.created") {
            manager->session = event["session"];
            //std::cout << json_data << std::endl;
        } else if (event["type"] == "session.updated") {
            manager->session = event["session"];
        } else if (event["type"] == "conversation.item.created") {
        } else if (event["type"] == "response.audio_transcript.delta") {
            //std::cout << event["delta"] << std::endl;
        } else if (event["type"] == "response.audio_transcript.done") {
            std::cout << event["transcript"] << std::endl;
        } else if (event["type"] == "response.done") {
            std::cout << event["response"]["usage"] << std::endl;
        } else if (event["type"] == "error") {
            std::cout << json_data << std::endl;
        } else {
            //std::cout << json_data << std::endl;
        }
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
}

void WebRTCManager::connectionTimeout() {
    request_exit = true;
}

void WebRTCManager::onConnectionStateChange(PeerConnectionState state,
                                             void *user_data) {
    WebRTCManager* manager = static_cast<WebRTCManager*>(user_data);
    printf("PeerConnectionState: %s\n",
             peer_connection_state_to_string(state));

    if (state == PEER_CONNECTION_DISCONNECTED) {
        printf("Connection disconnected!\n");
    } else if (state == PEER_CONNECTION_CLOSED) {
        printf("Connection close!\n");
        manager->request_exit = true;
    } else if (state == PEER_CONNECTION_CONNECTED) {
    } else if (state == PEER_CONNECTION_COMPLETED) {
        printf("Connection completed!\n");
    }
}

void WebRTCManager::onIceCandidate(char *description, void *user_data) {
    WebRTCManager* manager = static_cast<WebRTCManager*>(user_data);
    char local_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
    while (oai_http_request(description, local_buffer) != 0) {
        sleep(5);
    }
    peer_connection_set_remote_description(manager->peer_connection, local_buffer);
}

bool WebRTCManager::init() {
    PeerConfiguration peer_connection_config = {
        .ice_servers = {},
        .audio_codec = CODEC_OPUS,
        .video_codec = CODEC_NONE,
        .datachannel = DATA_CHANNEL_STRING,
        .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {
            oai_audio_decode(data, size);
        },
        .onvideotrack = NULL,
        .on_request_keyframe = NULL,
        .user_data = NULL,
    };
  
    peer_init();
    peer_connection = peer_connection_create(&peer_connection_config);
    if (peer_connection == NULL) {
        printf("Failed to create peer connection");
        return false;
    }
  
    peer_connection_config.user_data = this;
    peer_connection_oniceconnectionstatechange(peer_connection,
                                               onConnectionStateChange);
    peer_connection_onicecandidate(peer_connection, onIceCandidate);
    peer_connection_create_offer(peer_connection);
    peer_connection_ondatachannel(peer_connection, onMessage, onOpen, onClose);
    return true;
}

WebRTCManager::WebRTCManager(ThreadTimer& timer) : timer(timer) {
}

void WebRTCManager::loop() {
    //timer.set(10, oai_connection_timeout);
    //timer.start();
    while (1) {
        peer_connection_loop(peer_connection);
        usleep(10000);
        if (request_exit) {
            std::cout << "connection destroy" << std::endl;
            request_exit = false;
            break;
        }
    }
    peer_connection_destroy(peer_connection);
    peer_deinit();
    //timer.stop();
}
