#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "main.h"

PeerConnection *peer_connection = NULL;

static void oai_onconnectionstatechange_task(PeerConnectionState state,
                                             void *user_data) {
  printf("PeerConnectionState: %s",
           peer_connection_state_to_string(state));

  if (state == PEER_CONNECTION_DISCONNECTED ||
      state == PEER_CONNECTION_CLOSED) {
  } else if (state == PEER_CONNECTION_CONNECTED) {
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
      .datachannel = DATA_CHANNEL_NONE,
      .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {
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

  while (1) {
    peer_connection_loop(peer_connection);
    usleep(15000);
  }
}
