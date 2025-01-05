#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curl.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define MAX_HTTP_OUTPUT_BUFFER 4096
#define LOG_TAG "HTTP_CLIENT"

typedef struct {
    char *data;
    size_t size;
} http_response_t;

static char *g_api_key = NULL; 
static char *g_base_url = NULL; 

int get_openai_baseurl_and_key() {
    if (g_api_key) {
        return 0;
    }

    g_api_key = getenv("OPENAI_API_KEY");
    if (g_api_key == NULL) {
        fprintf(stderr, "Error: OPENAI_API_KEY not set in environment.\n");
        return 1;
    }
    g_base_url = getenv("OPENAI_REALTIMEAPI");
    if (g_base_url == NULL) {
        fprintf(stderr, "Error: base url not set in environment.\n");
        return 1;
    }

    printf("OpenAI API Key: %s\n", g_api_key);
    printf("Base url: %s\n", g_base_url);
    return 0;
}

// Callback function for handling HTTP response data
static size_t http_response_handler(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    http_response_t *resp = (http_response_t *)userp;

    char *ptr = (char*)realloc(resp->data, resp->size + total_size + 1);
    if (ptr == NULL) {
        fprintf(stderr, "%s: Failed to allocate memory\n", LOG_TAG);
        return 0;
    }

    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, total_size);
    resp->size += total_size;
    resp->data[resp->size] = '\0';

    return total_size;
}

// Function to send HTTP POST request
void oai_http_request(const char *offer, char *answer) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;

    http_response_t response;
    response.data = (char*)malloc(1); // Initial allocation for response data
    response.size = 0;

    // Initialize CURL
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "%s: Failed to initialize CURL\n", LOG_TAG);
        free(response.data);
        return;
    }

    get_openai_baseurl_and_key();
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, g_base_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, offer);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(offer));
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);


    // Add headers
    snprintf(answer, MAX_HTTP_OUTPUT_BUFFER, "Authorization: Bearer %s", g_api_key);
    headers = curl_slist_append(headers, "Content-Type: application/sdp");
    headers = curl_slist_append(headers, answer); // Authorization header
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set callback function to handle the response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_response_handler);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    // Perform the HTTP request
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "%s: CURL error: %s\n", LOG_TAG, curl_easy_strerror(res));
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 201) {
            fprintf(stderr, "%s: HTTP request failed, code: %ld\n", LOG_TAG, http_code);
        } else {
            strncpy(answer, response.data, MAX_HTTP_OUTPUT_BUFFER);
        }
    }

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(response.data);
}

