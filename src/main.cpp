#include "main.h"
#include <peer.h>

int main(void) {
  peer_init();
  oai_init_audio_capture();
  oai_init_audio_decoder();
  oai_webrtc();
}
