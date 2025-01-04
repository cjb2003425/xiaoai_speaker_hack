#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asoundlib.h"
#include "opus.h"
#include "main.h"

#define OPUS_OUT_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode
#define SAMPLE_RATE 8000
#define BUFFER_SAMPLES 320

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

snd_pcm_t *pcm_handle_input;
snd_pcm_t *pcm_handle_output;
opus_int16 *output_buffer = NULL;
OpusDecoder *opus_decoder = NULL;
OpusEncoder *opus_encoder = NULL;
opus_int16 *encoder_input_buffer = NULL;
uint8_t *encoder_output_buffer = NULL;

void oai_init_audio_capture() {
    const char *capture_name = "hw:0,2"; // Card 0, Device 2
    const char *playback_name = "hw:0,2"; // Card 0, Device 2
    // Initialize ALSA for output
    int err;
    if ((err = snd_pcm_open(&pcm_handle_output, capture_name, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "Unable to open playback device: %s\n", snd_strerror(err));
        return;
    }

    // Set parameters for output
    snd_pcm_set_params(pcm_handle_output, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, SAMPLE_RATE, 1, 500000); // 0.5 seconds

    // Initialize ALSA for input
    if ((err = snd_pcm_open(&pcm_handle_input, playback_name, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "Unable to open capture device: %s\n", snd_strerror(err));
        return;
    }

    // Set parameters for input
    snd_pcm_set_params(pcm_handle_input, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, SAMPLE_RATE, 1, 500000); // 0.5 seconds
}

void oai_init_audio_decoder() {
    int decoder_error = 0;
    opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, &decoder_error);
    if (decoder_error != OPUS_OK) {
        fprintf(stderr, "Failed to create OPUS decoder\n");
        return;
    }

    output_buffer = (opus_int16 *)malloc(BUFFER_SAMPLES * sizeof(opus_int16));
}

void oai_audio_decode(uint8_t *data, size_t size) {
    int decoded_size = opus_decode(opus_decoder, data, size, output_buffer, BUFFER_SAMPLES, 0);
    if (decoded_size > 0) {
        snd_pcm_writei(pcm_handle_output, output_buffer, decoded_size);
    }
}

void oai_init_audio_encoder() {
    int encoder_error;
    opus_encoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &encoder_error);
    if (encoder_error != OPUS_OK) {
        fprintf(stderr, "Failed to create OPUS encoder\n");
        return;
    }

    opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
    opus_encoder_ctl(opus_encoder, OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
    opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    encoder_input_buffer = (opus_int16 *)malloc(BUFFER_SAMPLES * sizeof(opus_int16));
    encoder_output_buffer = (uint8_t *)malloc(OPUS_OUT_BUFFER_SIZE);
}

void oai_send_audio(PeerConnection* peer_connection) {
    int bytes_read = snd_pcm_readi(pcm_handle_input, encoder_input_buffer, BUFFER_SAMPLES);
    if (bytes_read < 0) {
        fprintf(stderr, "Error reading from input device: %s\n", snd_strerror(bytes_read));
        return;
    }

    int encoded_size = opus_encode(opus_encoder, encoder_input_buffer, bytes_read / 2, encoder_output_buffer, OPUS_OUT_BUFFER_SIZE);
    peer_connection_send_audio(peer_connection, encoder_output_buffer, encoded_size);  
}