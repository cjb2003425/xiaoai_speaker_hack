#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curl.h"
#include "main.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define OPENAI_REALTIMEAPI "https://api.openai.com/v1/realtimeapi"
#define OPENAI_API_KEY "your_api_key_here"

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = (char *)realloc(mem->memory, mem->size + total_size + 1);
    if (ptr == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, total_size);
    mem->size += total_size;
    mem->memory[mem->size] = 0;

    return total_size;
}

void oai_http_request(char *offer, char *answer) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;

    struct MemoryStruct chunk;
    chunk.memory = (char*)malloc(1); // Initial allocation
    chunk.size = 0;

    snprintf(answer, MAX_HTTP_OUTPUT_BUFFER, "Bearer %s", OPENAI_API_KEY);

    curl = curl_easy_init();
    if (curl) {
        // Set the URL
        curl_easy_setopt(curl, CURLOPT_URL, OPENAI_REALTIMEAPI);

        // Set the HTTP POST method
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // Set the POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, offer);

        // Add necessary headers
        headers = curl_slist_append(headers, "Content-Type: application/sdp");
        headers = curl_slist_append(headers, answer);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set callback function to capture response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        // Perform the request
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 201) {
                printf("HTTP request successful, response: %s\n", chunk.memory);
            } else {
                printf("Error: HTTP request failed with status code %ld\n", response_code);
            }
        }

        // Cleanup
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(chunk.memory);
    } else {
        printf("Error: Failed to initialize curl");
    }
}

