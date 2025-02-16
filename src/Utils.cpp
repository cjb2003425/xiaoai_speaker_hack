#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>
#include <algorithm>
#include <curl/curl.h>
#include <random>
#include "Utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "AudioBuffer.h"

static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::pair<std::unique_ptr<uint8_t[]>, size_t> Utils::base64_decode(const std::string& in) {
    if (in.empty()) return {nullptr, 0};

    size_t padding = (in.back() == '=' ? (in[in.length()-2] == '=' ? 2 : 1) : 0);
    size_t out_len = ((in.length() * 3) / 4) - padding;
    auto out = std::make_unique<uint8_t[]>(out_len);
    size_t out_pos = 0;

    uint32_t buf = 0;
    int bits_left = 0;

    for (char c : in) {
        if (c == '=' || isspace(c)) continue;
        
        size_t val = base64_chars.find(c);
        if (val == std::string::npos) return {nullptr, 0};

        buf = (buf << 6) | val;
        bits_left += 6;
        
        if (bits_left >= 8) {
            bits_left -= 8;
            out[out_pos++] = (buf >> bits_left) & 0xFF;
        }
    }

    return {std::move(out), out_len};
}

std::string Utils::generateId(const std::string& prefix, size_t length) {
    const std::string chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, chars.size() - 1);

    std::string str = prefix;
    for (size_t i = 0; i < length - prefix.size(); ++i) {
        str += chars[distribution(generator)];
    }
    return str;
}

void Utils::base64DecodeAudio(const std::string &input, AudioBuffer& buffer) {
    std::pair<std::unique_ptr<uint8_t[]>, size_t> decoded = base64_decode(input);
    buffer.append(decoded.first.get(), decoded.second);
}

/// @brief Retrieves OpenAI API key from environment variables
/// @param[out] res Reference to store the API key
/// @return true if key was found and stored successfully, false otherwise
/// @throws std::runtime_error if environment variable access fails
bool Utils::get_openai_key(std::string& res) {
    const char* api_key = getenv("OPENAI_API_KEY");
    if (!api_key || std::string(api_key).empty()) {
        fprintf(stderr, "[Error]: OPENAI_API_KEY not set or empty in environment.\n");
        return false;
    }
    res.assign(api_key);
    return true;
}

/// @brief Retrieves OpenAI base URL from environment variables
/// @param[out] res Reference to store the base URL
/// @return true if URL was found and stored successfully, false otherwise
/// @throws std::runtime_error if environment variable access fails
bool Utils::get_openai_baseurl(std::string& res) {
    const char* base_url = getenv("OPENAI_REALTIMEAPI");
    if (!base_url || std::string(base_url).empty()) {
        fprintf(stderr, "[Error]: OPENAI_REALTIMEAPI not set or empty in environment.\n");
        return false;
    }
    res.assign(base_url);
    return true;
}

