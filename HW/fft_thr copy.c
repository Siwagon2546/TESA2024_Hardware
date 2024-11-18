#include "sound_app.h"
#include "sound_freq.h"
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite3.h>  // Include the SQLite3 header

#define SAMPLE_RATE 48000       // 48 kHz sample rate
#define BUFFER_DURATION 4       // Buffer 4 seconds of data
#define N 4096                  // FFT size
#define COOLDOWN_PERIOD 3       // Cooldown in seconds
#define SAVE_DURATION 2         // Duration of data to save before and after detection (in seconds)

const double low_interestfreq = 180.0;
const double high_interestfreq = 180.0;

// Ring buffer to hold the last 4 seconds of audio data
double ring_buffer[SAMPLE_RATE * BUFFER_DURATION];
int ring_buffer_index = 0;

// SQLite database pointer
sqlite3 *db;

void save_data_to_db(double *data, int size, const char *timestamp) {
    // Prepare SQL statement to insert data into the database
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO sound_data (timestamp, data) VALUES (?, ?)";
    
    // Prepare the SQL statement
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Bind the timestamp and data to the prepared statement
    sqlite3_bind_text(stmt, 1, timestamp, -1, SQLITE_STATIC);
    
    // Convert the sound data array into a string or blob for storage
    sqlite3_bind_blob(stmt, 2, data, size * sizeof(double), SQLITE_STATIC);

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }

    // Finalize the statement
    sqlite3_finalize(stmt);
}


void recognize_sound(double freq1, double freq2, int *count, time_t *last_detection_time) {
    bool c1 = false, c2 = false;
    time_t current_time = time(NULL);

    if (difftime(current_time, *last_detection_time) < COOLDOWN_PERIOD) {
        return;
    }

    if (fabs(freq1 - low_interestfreq) < 30.0) {
        c1 = true;
    }
    if (fabs(freq2 - low_interestfreq) < 30.0) {
        c1 = true;
    }

    if (fabs(freq1 - high_interestfreq) < 100.0) {
        c2 = true;
    }
    if (fabs(freq2 - high_interestfreq) < 100.0) {
        c2 = true;
    }

    if (c1 && c2) {
        (*count)++;
        printf("Sound detected! Count: %d\n", *count);
        
        // Save data from 2 seconds before and 2 seconds after detection
        int save_samples = SAMPLE_RATE * SAVE_DURATION;
        double save_buffer[2 * save_samples];

        // Copy 2 seconds of data before detection from ring buffer
        for (int i = 0; i < save_samples; i++) {
            int index = (ring_buffer_index - save_samples + i + SAMPLE_RATE * BUFFER_DURATION) % (SAMPLE_RATE * BUFFER_DURATION);
            save_buffer[i] = ring_buffer[index];
        }

        // Wait for 2 seconds to capture data after detection
        sleep(SAVE_DURATION);

        // Copy 2 seconds of data after detection from ring buffer
        for (int i = 0; i < save_samples; i++) {
            int index = (ring_buffer_index + i) % (SAMPLE_RATE * BUFFER_DURATION);
            save_buffer[save_samples + i] = ring_buffer[index];
        }

        // Get the current timestamp
        char timestamp[50];
        snprintf(timestamp, sizeof(timestamp), "%ld", time(NULL));

        // Save to the database
        save_data_to_db(save_buffer, 2 * save_samples, timestamp);

        printf("Data saved to DB with timestamp: %s, size: %d samples\n", timestamp, 2 * save_samples);

        *last_detection_time = current_time;
    }
}

int initialize_db(const char *db_name) {
    // Open SQLite database
    int rc = sqlite3_open(db_name, &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    // Create a table if it doesn't exist
    const char *sql_create_table = "CREATE TABLE IF NOT EXISTS sound_data ("
                                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                   "timestamp TEXT NOT NULL, "
                                   "data BLOB NOT NULL);";

    // Execute the create table SQL statement
    char *err_msg = NULL;
    rc = sqlite3_exec(db, sql_create_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }

    return SQLITE_OK;
}

void close_db() {
    // Close the SQLite database connection
    if (db) {
        sqlite3_close(db);
    }
}

void *fft_thr_fcn(void *ptr) {
    static double tmp_buf[N];
    static double freq_buf[N];
    double fs = SAMPLE_RATE;
    int count = 0;
    time_t last_detection_time = 0;
    initialize_db("sound_data.db");

    while (1) {
        pthread_mutex_lock(&data_cond_mutex);
        pthread_cond_wait(&data_cond, &data_cond_mutex);

        // Convert raw data to double (normalized) and store in ring buffer
        for (int i = 0; i < N; i++) {
            tmp_buf[i] = (double)shared_buf[i] / SHRT_MAX;
            ring_buffer[ring_buffer_index] = tmp_buf[i];
            ring_buffer_index = (ring_buffer_index + 1) % (SAMPLE_RATE * BUFFER_DURATION);
        }

        sound_freq(tmp_buf, freq_buf);

        // Find the top two dominant frequencies
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
