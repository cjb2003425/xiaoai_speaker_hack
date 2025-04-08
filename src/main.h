#include <string>
#include <alsa/asoundlib.h>
#include "RealTimeClient.h"
#define LOG_TAG "realtimeapi-sdk"
class ThreadTimer; 

RealTimeClient* oai_get_client(void);
void oai_init_audio_alsa(uint32_t);
ssize_t oai_audio_write(const void* data, snd_pcm_uframes_t size); 
void oai_init_audio_decoder(uint32_t sample_rate);
void oai_init_audio_encoder(uint32_t sample_rate);
void oai_audio_decode(uint8_t *data, size_t size);
int oai_http_request(const char *offer, char *answer);