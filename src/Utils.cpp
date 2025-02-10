#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>
#include <algorithm>
#include <random>
#include "Utils.h"

std::vector<int16_t> Utils::floatTo16BitPCM(const std::vector<float>& float32Array) {
    std::vector<int16_t> int16Array(float32Array.size());
    for (size_t i = 0; i < float32Array.size(); ++i) {
        float s = std::max(-1.0f, std::min(1.0f, float32Array[i]));
        int16Array[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
    }
    return int16Array;
}

std::vector<uint8_t> Utils::base64ToArrayBuffer(const std::string& base64) {
    std::string binaryString = base64_decode(base64);
    std::vector<uint8_t> bytes(binaryString.begin(), binaryString.end());
    return bytes;
}

std::string Utils::arrayBufferToBase64(const std::vector<uint8_t>& arrayBuffer) {
    std::string binary(arrayBuffer.begin(), arrayBuffer.end());
    return base64_encode(binary);
}

std::vector<int16_t> Utils::mergeInt16Arrays(const std::vector<int16_t>& left, const std::vector<int16_t>& right) {
    std::vector<int16_t> merged(left.size() + right.size());
    std::copy(left.begin(), left.end(), merged.begin());
    std::copy(right.begin(), right.end(), merged.begin() + left.size());
    return merged;
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

std::string Utils::base64_encode(const std::string &in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string Utils::base64_decode(const std::string &in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
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

