#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <string>
#include "Utils.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define MAX_HTTP_OUTPUT_BUFFER 4096
#define LOG_TAG "HTTP_CLIENT"

typedef struct {
    char *data;
    size_t size;
} http_response_t;

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
int oai_http_request(const char *offer, char *answer) {
    int ret = -1;
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
        return -1;
    }

    std::string base_url;
    std::string api_key;

    if (!Utils::get_openai_baseurl(base_url)) {
        return -1;
    }

    if (!Utils::get_openai_key(api_key)) {
        return -1;
    }

    if (base_url.empty() || api_key.empty()) { // Temporary check
        fprintf(stderr, "%s: Failed to retrieve OpenAI API credentials\n", LOG_TAG);
        free(response.data);
        curl_easy_cleanup(curl);
        return -1;
    }

    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, base_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, offer);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(offer));
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);


    // Add headers
    snprintf(answer, MAX_HTTP_OUTPUT_BUFFER, "Authorization: Bearer %s", api_key.c_str());
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
        ret = -1;
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 201) {
            fprintf(stderr, "%s: HTTP request failed, code: %ld\n", LOG_TAG, http_code);
            ret = -1;
        } else {
            strncpy(answer, response.data, MAX_HTTP_OUTPUT_BUFFER);
            ret = 0;
        }
    }

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(response.data);
    return ret;
}
