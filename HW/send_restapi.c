#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <cjson/cJSON.h>
#include "send_restapi.h"
#include "db_helper.h"

// Function to Base64 encode binary data
char *base64_encode(const unsigned char *input, int length) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new(BIO_s_mem());
    if (!b64 || !bio) {
        fprintf(stderr, "Failed to initialize BIO for Base64 encoding.\n");
        return NULL;
    }

    b64 = BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);

    BUF_MEM *buffer;
    BIO_get_mem_ptr(b64, &buffer);

    char *encoded_data = malloc(buffer->length + 1);
    if (encoded_data == NULL) {
        fprintf(stderr, "Failed to allocate memory for Base64 encoding.\n");
    } else {
        memcpy(encoded_data, buffer->data, buffer->length);
        encoded_data[buffer->length] = '\0';
    }

    BIO_free_all(b64);
    return encoded_data;
}

// Function to send the sample array as Base64 encoded data via REST API
int sendrest_api(char *json_data) {
    CURL *curl;
    CURLcode res;
    
    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // Define the URL for the API
        const char *url = "http://192.168.218.235:5000/audio/sample";

        // Set the URL for the POST request
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Authorization: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6MSwibmFtZSI6ImhvbW15IiwiaWF0IjoxNzMxNjQzMzY2LCJleHAiOjE3MzE3Mjk3NjZ9.6KjQUTUymV9lk2d271epqA7ilMtzruuKbrT-BZppjf0");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set the JSON data as the POST body
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            //printf("Data posted successfully.\n");
        }

        // Clean up
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "Failed to initialize cURL.\n");
    }

    // Cleanup cURL globally
    curl_global_cleanup();

    // Free allocated memory
    

    return 1;
}
