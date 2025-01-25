#include <string>
#include "peer.h"

#define LOG_TAG "realtimeapi-sdk"

void oai_wifi(void);
void oai_init_audio_capture(void);
void oai_init_audio_decoder(void);
void oai_init_audio_encoder();
void oai_send_audio(PeerConnection *peer_connection);
void oai_audio_decode(uint8_t *data, size_t size);
void oai_webrtc();
void oai_http_request(const char *offer, char *answer);
void create_conversation_item(std::string& message);
void create_response();