#include <string>
#include <map>
#include <iostream>
#include <curl/curl.h>
#include <memory>
#include <sstream>
#include "HttpAPI.h"

// Callback function to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Helper function to perform CURL requests
std::string HttpAPI::performRequest(const std::string& url, const std::string& method, 
                         const std::map<std::string, std::string>& headers) {
    CURL* curl = curl_easy_init();
    std::string response_string;
    
    if (curl) {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Set callback function
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // Set method
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
        }

        // Set headers
        struct curl_slist* chunk = NULL;
        for (const auto& header : headers) {
            std::string headerStr = header.first + ": " + header.second;
            chunk = curl_slist_append(chunk, headerStr.c_str());
        }
        
        if (chunk) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        }

        // Perform request
        CURLcode res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            std::string error = "curl_easy_perform() failed: ";
            error += curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            if (chunk) {
                curl_slist_free_all(chunk);
            }
            throw std::runtime_error(error);
        }

        // Cleanup
        curl_easy_cleanup(curl);
        if (chunk) {
            curl_slist_free_all(chunk);
        }
    }

    return response_string;
}

// Add bool parameter to match header
std::map<std::string, std::string> HttpAPI::getHeaders() {
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["x-api-key"] = key;
    return headers;
}

// Remove default argument here
std::string HttpAPI::getData(const std::map<std::string, std::string>& filters) {
    // Build URL with filters
    if (!filters.empty()) {
        url += "?";
        for (const auto& filter : filters) {
            url += filter.first + "=" + filter.second + "&";
        }
        url.pop_back(); // Remove last '&'
    }

    std::cout << "Making request to: " << url << std::endl;

    try {
        return performRequest(url, "GET", getHeaders());
    } catch (const std::exception& e) {
        std::cerr << "Data fetch failed: " << e.what() << std::endl;
        throw;
    }
}

HttpAPI::HttpAPI(const std::string& url, const std::string& key) {
    this->url= url;
    this->key = key;
}