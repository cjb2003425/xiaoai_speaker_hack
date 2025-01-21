#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp> 
#include "main.h"
#include "session.h"

// For convenience
using json = nlohmann::json;

// Define constants
#define TICK_INTERVAL 15
#define DATACHANNEL_NAME "oai-events"
#define MAX_EVENT_NAME_LEN 64
#define MAX_HTTP_OUTPUT_BUFFER 4096

char* protocol = "bar";

PeerConnection *peer_connection = nullptr;
Session* session = nullptr;

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
        // Check if the type is "session.created"
        if (event["type"] == "session.created") {
            session = new Session(peer_connection);
            session->from_json(event["session"]);
            // Print the session details
            session->print();
            session->created();
            session->update();
        } else {
            std::cout << "The type is not 'session.created'. No session object created." << std::endl;
        }
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
}

static void oai_onconnectionstatechange_task(PeerConnectionState state,
                                             void *user_data) {
  printf("PeerConnectionState: %s\n",
           peer_connection_state_to_string(state));

  if (state == PEER_CONNECTION_DISCONNECTED ||
      state == PEER_CONNECTION_CLOSED) {
      printf("Connection fail!");
  } else if (state == PEER_CONNECTION_CONNECTED) {
#if 0
      pthread_t audio_thread;
      int result = pthread_create(&audio_thread, NULL, oai_send_audio_task, NULL);
      if (result != 0) {
          // Handle the error
          perror("Failed to create audio thread");
          exit(1); // Exit or handle the error as appropriate
      }
#endif
  }
}

static void oai_on_icecandidate_task(char *description, void *user_data) {
  char local_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
  oai_http_request(description, local_buffer);
  peer_connection_set_remote_description(peer_connection, local_buffer);
}

void oai_webrtc() {
  PeerConfiguration peer_connection_config = {
      .ice_servers = {},
      .audio_codec = CODEC_OPUS,
      .video_codec = CODEC_NONE,
      .datachannel = DATA_CHANNEL_STRING,
      .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {
        //oai_audio_decode(data, size);
      },
      .onvideotrack = NULL,
      .on_request_keyframe = NULL,
      .user_data = NULL,
  };

  peer_connection = peer_connection_create(&peer_connection_config);
  if (peer_connection == NULL) {
    printf("Failed to create peer connection");
  }

  peer_connection_oniceconnectionstatechange(peer_connection,
                                             oai_onconnectionstatechange_task);
  peer_connection_onicecandidate(peer_connection, oai_on_icecandidate_task);
  peer_connection_create_offer(peer_connection);
  peer_connection_ondatachannel(peer_connection, on_message, on_open, on_close);

  while (1) {
    peer_connection_loop(peer_connection);
    usleep(15000);
  }
}
