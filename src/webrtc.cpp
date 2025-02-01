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

// For convenience
using json = nlohmann::json;

// Define constants
#define TICK_INTERVAL 15
#define DATACHANNEL_NAME "oai-events"
#define MAX_EVENT_NAME_LEN 64
#define MAX_HTTP_OUTPUT_BUFFER 4096

char* protocol = "bar";

PeerConnection *peer_connection = nullptr;
json session;
json conversation;
bool request_exit = false;

void create_conversation_item(std::string& message) {
    std::string jsonString = R"(
    {
        "type": "conversation.item.create",
        "item": {
            "type": "message",
            "role": "user",
            "content": [
                {
                    "type": "input_text"
                }
            ]
        }
    }
    )";
    json item = json::parse(jsonString);
    item["item"]["content"][0]["text"] = message;
    auto data = item.dump();
    peer_connection_datachannel_send(peer_connection, const_cast<char*>(data.c_str()), data.size());
}

void create_response() {
    std::string jsonString = R"({
      "type": "response.create",
      "response": {
          "modalities": [ "text", "audio"]
      }
    })";
    peer_connection_datachannel_send(peer_connection, const_cast<char*>(jsonString.c_str()), jsonString.size());
}

// Audio sending task
void *oai_send_audio_task(void *user_data) {
    oai_init_audio_encoder();
    while (1) {
        oai_send_audio(peer_connection);
        usleep(TICK_INTERVAL * 1000); // Sleep for TICK_INTERVAL milliseconds
    }
    return NULL;
}

void on_open(void* userdata) {
  printf("on open\n");
  if (peer_connection_create_datachannel(peer_connection, DATA_CHANNEL_RELIABLE, 0, 0, DATACHANNEL_NAME, protocol) == 13) {
     printf("data channel created\n"); 
  }
}

void on_close(void* userdata) {
  printf("on close\n");
}

void on_message(char* msg, size_t len, void* userdata, uint16_t sid) {
    std::string json_data = std::string(msg, len);
    try {
        // Parse the JSON message
        json event = json::parse(json_data);
        //std::cout << "message type is " << event["type"] << std::endl;
        if (event["type"] == "session.created") {
            session = event["session"];
            //std::cout << json_data << std::endl;
        } else if (event["type"] == "session.updated") {
            session = event["session"];
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

static void oai_connection_timeout() {
    request_exit = true;
}

static void oai_onconnectionstatechange_task(PeerConnectionState state,
                                             void *user_data) {
    printf("PeerConnectionState: %s\n",
             peer_connection_state_to_string(state));

    if (state == PEER_CONNECTION_DISCONNECTED) {
        printf("Connection disconnected!\n");
    } else if (state == PEER_CONNECTION_CLOSED) {
        printf("Connection close!\n");
        request_exit = true;
    } else if (state == PEER_CONNECTION_CONNECTED) {
    } else if (state == PEER_CONNECTION_COMPLETED) {
        printf("Connection completed!\n");
    }
}

static void oai_on_icecandidate_task(char *description, void *user_data) {
    char local_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
    while (oai_http_request(description, local_buffer) != 0) {
        sleep(5);
    }
    peer_connection_set_remote_description(peer_connection, local_buffer);
}

void oai_webrtc(ThreadTimer& timer) {
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
    }
  
    peer_connection_oniceconnectionstatechange(peer_connection,
                                               oai_onconnectionstatechange_task);
    peer_connection_onicecandidate(peer_connection, oai_on_icecandidate_task);
    peer_connection_create_offer(peer_connection);
    peer_connection_ondatachannel(peer_connection, on_message, on_open, on_close);
    
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
