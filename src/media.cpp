#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <iostream>
#include <opus/opus.h>
#include <time.h>
#include "peer.h"

#define OPUS_OUT_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode
#define SAMPLE_RATE 8000
#define BUFFER_SAMPLES 320
#define PCM_DEVICE "default" // Default ALSA playback device

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

snd_pcm_t *pcm_handle_input;
snd_pcm_t *pcm_handle_output;
opus_int16 *output_buffer = NULL;
OpusDecoder *opus_decoder = NULL;
OpusEncoder *opus_encoder = NULL;
opus_int16 *encoder_input_buffer = NULL;
uint8_t *encoder_output_buffer = NULL;
FILE *output_file = NULL;

void oai_init_audio_alsa(uint32_t sample_rate) {
    snd_pcm_hw_params_t *params;
    int channels = 1;
    snd_pcm_uframes_t frames = BUFFER_SAMPLES;
    snd_pcm_uframes_t buffer_size;
    char *buffer;
    int pcm, dir;

    // Open the PCM device
    if ((pcm = snd_pcm_open(&pcm_handle_output, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "ERROR: Can't open \"%s\" PCM device. %s\n", PCM_DEVICE, snd_strerror(pcm));
        return;
    }

    // Allocate hardware parameters object
    snd_pcm_hw_params_alloca(&params);

    // Set default parameters
    snd_pcm_hw_params_any(pcm_handle_output, params);

    // Set access type (interleaved)
    snd_pcm_hw_params_set_access(pcm_handle_output, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // Set sample format (16-bit signed little-endian)
    snd_pcm_hw_params_set_format(pcm_handle_output, params, SND_PCM_FORMAT_S16_LE);

    // Set sample rate
    snd_pcm_hw_params_set_rate_near(pcm_handle_output, params, &sample_rate, &dir);

    // Set number of channels
    snd_pcm_hw_params_set_channels(pcm_handle_output, params, channels);

    snd_pcm_hw_params_set_period_size_near(pcm_handle_output, params, &frames, &dir);

    // Apply hardware parameters to PCM device
    if ((pcm = snd_pcm_hw_params(pcm_handle_output, params)) < 0) {
        fprintf(stderr, "ERROR: Can't set hardware parameters. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle_output);
        return;
    }
}

void oai_init_audio_decoder() {
    int decoder_error = 0;
    opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, &decoder_error);
    if (decoder_error != OPUS_OK) {
        fprintf(stderr, "Failed to create OPUS decoder\n");
        return;
    }

    #ifdef AUDIO_DEBUG
    // Open the file for writing binary data
    output_file = fopen("/tmp/ai.wav", "ab"); // Append binary mode
    if (output_file == NULL) {
        perror("Failed to open output file");
        return;
    }
    #endif

    output_buffer = (opus_int16 *)malloc(BUFFER_SAMPLES * sizeof(opus_int16));
}

ssize_t oai_audio_write(const void* data, snd_pcm_uframes_t size) {
    ssize_t res = 0;
    res = snd_pcm_writei(pcm_handle_output, data, size);
    if (res == -EPIPE) {
        // Buffer underrun
        fprintf(stderr, "XRUN.\n");
        snd_pcm_prepare(pcm_handle_output);
    } else if (res < 0) {
        fprintf(stderr, "ERROR: Can't write to PCM device. %s\n", snd_strerror(res));
    } else {
    }
    return res;
}

void oai_audio_decode(uint8_t *data, size_t size) {
    ssize_t res = 0;
    int decoded_size = opus_decode(opus_decoder, data, size, output_buffer, BUFFER_SAMPLES, 0);
    if (decoded_size > 0) {
        #ifdef AUDIO_DEBUG
        //size_t written = fwrite(output_buffer, sizeof(int16_t), decoded_size, output_file);
        if (written != (size_t)decoded_size) {
            fprintf(stderr, "Failed to write all decoded data to file\n");
        }
        #endif
        res = oai_audio_write(output_buffer, decoded_size);
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