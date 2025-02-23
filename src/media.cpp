#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <iostream>
#include <opus/opus.h>
#include <time.h>
#include "peer.h"
#include <cmath>

#define OPUS_OUT_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode
#define SAMPLE_RATE 8000
#define BUFFER_SAMPLES 320
#define PCM_DEVICE "default" // Default ALSA playback device

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0
#define VOLUME_GAIN 1.6f  // Define a gain factor to increase the volume
#define BUFFER_TIME 1000000   // Increase to 1000ms buffer
#define PERIOD_TIME 20000    // Decrease to 20ms period for more frequent updates
#define START_THRESHOLD 0.6   

snd_pcm_t *pcm_handle_input;
snd_pcm_t *pcm_handle_output;
opus_int16 *output_buffer = NULL;
OpusDecoder *opus_decoder = NULL;
OpusEncoder *opus_encoder = NULL;
opus_int16 *encoder_input_buffer = NULL;
uint8_t *encoder_output_buffer = NULL;
FILE *output_file = NULL;
static snd_pcm_uframes_t buffer_size = 0;

void oai_init_audio_alsa(uint32_t sample_rate) {
    snd_pcm_hw_params_t *params;
    int channels = 1;
    unsigned int buffer_time = BUFFER_TIME;
    unsigned int period_time = PERIOD_TIME;
    int pcm, dir;

    // Open the PCM device
    if ((pcm = snd_pcm_open(&pcm_handle_output, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "ERROR: Can't open \"%s\" PCM device. %s\n", PCM_DEVICE, snd_strerror(pcm));
        return;
    }

    // Allocate hardware parameters object
    snd_pcm_hw_params_alloca(&params);

    // Fill params with default values
    snd_pcm_hw_params_any(pcm_handle_output, params);

    // Set access type
    snd_pcm_hw_params_set_access(pcm_handle_output, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // Set format
    snd_pcm_hw_params_set_format(pcm_handle_output, params, SND_PCM_FORMAT_S16_LE);

    // Set channels
    snd_pcm_hw_params_set_channels(pcm_handle_output, params, channels);

    // Set sample rate
    snd_pcm_hw_params_set_rate_near(pcm_handle_output, params, &sample_rate, &dir);

    // Set buffer time and period time
    snd_pcm_hw_params_set_buffer_time_near(pcm_handle_output, params, &buffer_time, &dir);
    snd_pcm_hw_params_set_period_time_near(pcm_handle_output, params, &period_time, &dir);

    // Apply hardware parameters
    if ((pcm = snd_pcm_hw_params(pcm_handle_output, params)) < 0) {
        fprintf(stderr, "ERROR: Can't set hardware parameters. %s\n", snd_strerror(pcm));
        snd_pcm_close(pcm_handle_output);
        return;
    }

    // Configure software parameters
    snd_pcm_sw_params_t *sw_params;
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(pcm_handle_output, sw_params);
    
    // Set start threshold (30% of buffer size)
    snd_pcm_sw_params_set_start_threshold(pcm_handle_output, sw_params, 
                                        (snd_pcm_uframes_t)(buffer_size * START_THRESHOLD));
    
    // Set minimum available frames to wake up from waiting
    snd_pcm_sw_params_set_avail_min(pcm_handle_output, sw_params, 
                                   (snd_pcm_uframes_t)(buffer_size * 0.5));
    
    // Apply software parameters
    if ((pcm = snd_pcm_sw_params(pcm_handle_output, sw_params)) < 0) {
        fprintf(stderr, "ERROR: Can't set software parameters. %s\n", snd_strerror(pcm));
        return;
    }

    // Prepare and start the PCM device
    snd_pcm_prepare(pcm_handle_output);
    snd_pcm_start(pcm_handle_output);
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
    opus_int16* buffer = (opus_int16*)data;
    for (snd_pcm_uframes_t i = 0; i < size; ++i) {
        buffer[i] = buffer[i] * VOLUME_GAIN;
        if (buffer[i] > INT16_MAX) buffer[i] = INT16_MAX;
        if (buffer[i] < INT16_MIN) buffer[i] = INT16_MIN;
    }
    res = snd_pcm_writei(pcm_handle_output, buffer, size);
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