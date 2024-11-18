#include "sound_app.h"
#include "sound_freq.h"
#include "db_helper.h"
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <cjson/cJSON.h>
#include "send_restapi.h"

#define SAMPLE_RATE 48000       // 48 kHz sample rate
#define BUFFER_DURATION 4       // Buffer 4 seconds of data
#define N 4096                  // FFT size
#define COOLDOWN_PERIOD 3       // Cooldown in seconds
#define SAVE_DURATION 1         // Duration of data to save before and after detection (in seconds)

const double low_interestfreq = 180.0;
const double high_interestfreq = 180.0;

// Ring buffer to hold the last 4 seconds of audio data
double ring_buffer[SAMPLE_RATE * BUFFER_DURATION];
int ring_buffer_index = 0;

// Error handling for REST API send function
int send_restapi_safe(const char *data) {
    if (data == NULL) {
        fprintf(stderr, "REST API data is null\n");
        return -1;
    }
    
    int retry = 3;
    while (retry--) {
        if (sendrest_api(data) == 0) {
            return 0;
        }
        fprintf(stderr, "Failed to send data to REST API, retrying...\n");
        sleep(1); // Delay before retrying
    }
    fprintf(stderr, "Failed to send data to REST API after retries\n");
    return -1;
}

void recognize_sound(double freq1, double freq2, int *count, time_t *last_detection_time) {
    bool c1 = false, c2 = false;
    time_t current_time = time(NULL);

    if (difftime(current_time, *last_detection_time) < COOLDOWN_PERIOD) {
        return;
    }

    if (fabs(freq1 - low_interestfreq) < 30.0) c1 = true;
    if (fabs(freq2 - low_interestfreq) < 30.0) c1 = true;

    if (fabs(freq1 - high_interestfreq) < 100.0) c2 = true;
    if (fabs(freq2 - high_interestfreq) < 100.0) c2 = true;

    if (c1 && c2) {
        (*count)++;
        printf("Sound detected! Count: %d\n", *count);
        
        int save_samples = SAMPLE_RATE * SAVE_DURATION;
        double save_buffer[2 * save_samples];

        // Copy pre-detection samples
        for (int i = 0; i < save_samples; i++) {
            int index = (ring_buffer_index - save_samples + i + SAMPLE_RATE * BUFFER_DURATION) % (SAMPLE_RATE * BUFFER_DURATION);
            save_buffer[i] = ring_buffer[index];
        }

        sleep(SAVE_DURATION);  // Wait for additional data

        // Copy post-detection samples
        for (int i = 0; i < save_samples; i++) {
            int index = (ring_buffer_index + i) % (SAMPLE_RATE * BUFFER_DURATION);
            save_buffer[save_samples + i] = ring_buffer[index];
        }

        // Base64 encode the sample buffer
        char *encoded_sample = base64_encode((unsigned char *)save_buffer, sizeof(save_buffer));
        if (encoded_sample == NULL) {
            fprintf(stderr, "Failed to encode samples in Base64.\n");
            return;
        }

        // Create JSON object
        cJSON *json = cJSON_CreateObject();
        if (!json) {
            fprintf(stderr, "Failed to create JSON object.\n");
            free(encoded_sample);
            return;
        }

        // Add elements to JSON object
        cJSON_AddStringToObject(json, "name", "Sample Audio");
        cJSON_AddNumberToObject(json, "samplerate", SAMPLE_RATE);
        cJSON_AddStringToObject(json, "channel", encoded_sample);

        // Convert JSON to string
        char *json_data = cJSON_PrintUnformatted(json);
        if (json_data) {
            printf("JSON data size: %zu bytes\n", strlen(json_data));

            // Send JSON via REST API safely
            send_restapi_safe(json_data);

            free(json_data);
        } else {
            fprintf(stderr, "Failed to convert JSON to string.\n");
        }
        dbase_append("sound_data.db","pump");
        // Clean up
        cJSON_Delete(json);
        free(encoded_sample);
        *last_detection_time = current_time;
    }
}

void *fft_thr_fcn(void *ptr) {
    static double tmp_buf[N];
    static double freq_buf[N];
    double fs = SAMPLE_RATE;
    int count = 0;
    
    time_t last_detection_time = 0;
    dbase_init("sound_data.db");
    if (dbase_init("sound_data.db") != 0) {
        fprintf(stderr, "Database initialization failed\n");
        pthread_exit(NULL);
    }

    while (1) {
        pthread_mutex_lock(&data_cond_mutex);
        
        if (pthread_cond_wait(&data_cond, &data_cond_mutex) != 0) {
            fprintf(stderr, "Failed to wait on condition variable\n");
            pthread_mutex_unlock(&data_cond_mutex);
            continue;
        }

        for (int i = 0; i < N; i++) {
            if (i >= N || ring_buffer_index >= SAMPLE_RATE * BUFFER_DURATION) {
                fprintf(stderr, "Buffer overflow or index out of bounds\n");
                break;
            }
            
            tmp_buf[i] = (double)shared_buf[i] / SHRT_MAX;
            ring_buffer[ring_buffer_index] = tmp_buf[i];
            ring_buffer_index = (ring_buffer_index + 1) % (SAMPLE_RATE * BUFFER_DURATION);
        }

        sound_freq(tmp_buf, freq_buf);

        double top_freqs[2] = {0.0, 0.0};
        double top_magnitudes[2] = {0.0, 0.0};

        for (int i = 0; i < N / 2; i++) {
            double magnitude = freq_buf[i];
            double frequency = (i * fs) / N;

            if (magnitude > top_magnitudes[0]) {
                top_magnitudes[1] = top_magnitudes[0];
                top_freqs[1] = top_freqs[0];
                top_magnitudes[0] = magnitude;
                top_freqs[0] = frequency;
            } else if (magnitude > top_magnitudes[1]) {
                top_magnitudes[1] = magnitude;
                top_freqs[1] = frequency;
            }
        }

        if (top_magnitudes[0] > 400) {
            recognize_sound(top_freqs[0], top_freqs[1], &count, &last_detection_time);
        }

        pthread_mutex_unlock(&data_cond_mutex);
    }
}
