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
#include "ThreadTimer.hpp"
#include "WebRTCClient.h"

// For convenience
using json = nlohmann::json;
using namespace std;

void WebRTCClient::onOpen(void* userdata) {
    WebRTCClient* client = static_cast<WebRTCClient*>(userdata);
    char name[32] = "oai-event";
    char protocol[32] = "bar";
    printf("on open\n");
    if (peer_connection_create_datachannel(client->peer_connection, DATA_CHANNEL_RELIABLE, 0, 0, name, protocol) == 13) {
       printf("data channel created\n"); 
    }
}

void WebRTCClient::onClose(void* userdata) {
    WebRTCClient* client = static_cast<WebRTCClient*>(userdata);
    printf("on close\n");
    client->retryRequest = true;
}

void WebRTCClient::onMessage(char* msg, size_t len, void* userdata, uint16_t sid) {
    WebRTCClient* client = static_cast<WebRTCClient*>(userdata);
    std::string json_data = std::string(msg, len);
    client->onMessage(json_data);
}

void WebRTCClient::connectionTimeout() {
}

void WebRTCClient::onConnectionStateChange(PeerConnectionState state,
                                             void *user_data) {
    WebRTCClient* client = static_cast<WebRTCClient*>(user_data);
    printf("PeerConnectionState: %s\n",
             peer_connection_state_to_string(state));

    if (state == PEER_CONNECTION_DISCONNECTED) {
        printf("Connection disconnected!\n");
    } else if (state == PEER_CONNECTION_CLOSED) {
        printf("Connection close!\n");
        client->retryRequest = true;
    } else if (state == PEER_CONNECTION_CONNECTED) {
    } else if (state == PEER_CONNECTION_COMPLETED) {
        printf("Connection completed!\n");
    }
}

void WebRTCClient::onIceCandidate(char *description, void *user_data) {
    WebRTCClient* client = static_cast<WebRTCClient*>(user_data);
    char local_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
    while (oai_http_request(description, local_buffer) != 0) {
        sleep(5);
    }
    peer_connection_set_remote_description(client->peer_connection, local_buffer);
}

bool WebRTCClient::init() {
    PeerConfiguration peer_connection_config = {
        .ice_servers = {},
        .audio_codec = CODEC_OPUS,
        .video_codec = CODEC_NONE,
        .datachannel = DATA_CHANNEL_STRING,
        .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {
            WebRTCClient* client = static_cast<WebRTCClient*>(userdata);
            if (!client->mute) {
                oai_audio_decode(data, size);
            }
        },
        .onvideotrack = NULL,
        .on_request_keyframe = NULL,
        .user_data = this,
    };
  
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

bool WebRTCClient::deinit() { 
    if (peer_connection) {
        peer_connection_destroy(peer_connection);
        peer_connection = nullptr;
    }
    return true;
}

WebRTCClient::WebRTCClient(ThreadTimer& timer) : 
    timer(timer) {
    peer_init();
}

WebRTCClient::~WebRTCClient() {
    peer_deinit();
};

bool WebRTCClient::loop() {
    while (1) {
        peer_connection_loop(peer_connection);
        usleep(10000);
        if (quitRequest) {
            std::cout << "connection destroy" << std::endl;
            quitRequest = false;
            break;
        }

        if (retryRequest) {
            init();
            retryRequest = false;
        }
    }
    return true;
}

bool WebRTCClient::sendMessage(const std::string& message) {
    if (peer_connection_datachannel_send(peer_connection, const_cast<char*>(message.c_str()), message.size()) < 0) {
        std::cout << "send message failed" << std::endl;
        return false;
    }
    return true;
}

void WebRTCClient::onMessage(std::string& message) {
    RealTimeClient::onMessage(message);
}