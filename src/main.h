#include <string>

#define LOG_TAG "realtimeapi-sdk"
class ThreadTimer; 

void oai_wifi(void);
void oai_init_audio_capture(void);
void oai_init_audio_decoder(void);
void oai_init_audio_encoder();
void oai_audio_decode(uint8_t *data, size_t size);
int oai_http_request(const char *offer, char *answer);