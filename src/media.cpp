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
#define BUFFER_SAMPLES 960  // Reduced to handle 30ms chunks
#define PCM_DEVICE "default" // Default ALSA playback device

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0
#define VOLUME_GAIN 1.0f  // Define a gain factor to increase the volume
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

void oai_init_audio_decoder(uint32_t sample_rate) {
    if (opus_decoder != NULL) {
        opus_decoder_destroy(opus_decoder);
        opus_decoder = NULL;
    }

    int decoder_error = 0;
    opus_decoder = opus_decoder_create(sample_rate, 1, &decoder_error);
    if (decoder_error != OPUS_OK) {
        fprintf(stderr, "Failed to create OPUS decoder: %d\n", decoder_error);
        return;
    }

    // Free existing buffer if any
    if (output_buffer != NULL) {
        free(output_buffer);
    }

    // Increase buffer size
    size_t buffer_size = BUFFER_SAMPLES * sizeof(opus_int16);  // 40ms at 24kHz
    output_buffer = (opus_int16 *)calloc(BUFFER_SAMPLES, sizeof(opus_int16));
    if (!output_buffer) {
        fprintf(stderr, "Failed to allocate output buffer (size: %zu bytes)\n", buffer_size);
        opus_decoder_destroy(opus_decoder);
        opus_decoder = NULL;
        return;
    }

    #ifdef AUDIO_DEBUG
    if (output_file != NULL) {
        fclose(output_file);
    }
    output_file = fopen("/tmp/ai.wav", "ab");
    if (output_file == NULL) {
        perror("Failed to open output file");
        free(output_buffer);
        output_buffer = NULL;
        opus_decoder_destroy(opus_decoder);
        opus_decoder = NULL;
        return;
    }
    #endif
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
    // Increase buffer size to handle larger Opus frames
    ssize_t res = 0;
    size_t offset = 0;
    
    if (!data || !output_buffer) {
        fprintf(stderr, "Invalid data or buffer in oai_audio_decode\n");
        return;
    }

    if (size < 2) {
        fprintf(stderr, "Packet too small to be valid Opus data (size: %zu)\n", size);
        return;
    }

    // Try to decode the entire packet at once first
    int decoded_size = opus_decode(opus_decoder, 
                                 data,
                                 size,
                                 output_buffer,
                                 BUFFER_SAMPLES,
                                 0);
    
    if (decoded_size > 0) {
        // Successfully decoded whole packet
        res = oai_audio_write(output_buffer, decoded_size);
        if (res < 0) {
            fprintf(stderr, "Failed to write audio: %s\n", snd_strerror(res));
        }
        return;
    }

    // If whole packet decode failed, try to find valid Opus frames
    while (offset < size) {
        // Look for Opus frame marker (0xFF)
        while (offset < size && data[offset] != 0xFF) {
            offset++;
        }
        
        if (offset + 1 >= size) {
            break;
        }

        // Try to determine frame size from header
        size_t frame_size = 0;
        if ((data[offset + 1] & 0xFC) == 0xFC) {
            frame_size = (data[offset + 1] & 0x03) * 64;
        }
        
        if (frame_size == 0 || offset + frame_size > size) {
            frame_size = size - offset;
        }

        decoded_size = opus_decode(opus_decoder,
                                 data + offset,
                                 frame_size,
                                 output_buffer,
                                 BUFFER_SAMPLES,
                                 0);

        if (decoded_size > 0) {
            res = oai_audio_write(output_buffer, decoded_size);
            if (res < 0) {
                fprintf(stderr, "Failed to write audio frame: %s\n", snd_strerror(res));
                return;
            }
            offset += frame_size;
        } else {
            // Skip this byte and continue searching
            offset++;
        }
    }
}

void oai_init_audio_encoder(uint32_t sample_rate) {
    int encoder_error;
    opus_encoder = opus_encoder_create(sample_rate, 1, OPUS_APPLICATION_VOIP, &encoder_error);
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
