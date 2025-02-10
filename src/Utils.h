#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <cstdint>

class Utils {
public:
    // Converts Float32Array of amplitude data to ArrayBuffer in Int16Array format
    static std::vector<int16_t> floatTo16BitPCM(const std::vector<float>& float32Array);

    // Converts a base64 string to an ArrayBuffer
    static std::vector<uint8_t> base64ToArrayBuffer(const std::string& base64);

    // Converts an ArrayBuffer, Int16Array or Float32Array to a base64 string
    static std::string arrayBufferToBase64(const std::vector<uint8_t>& arrayBuffer);

    // Merge two Int16Arrays from Int16Arrays or ArrayBuffers
    static std::vector<int16_t> mergeInt16Arrays(const std::vector<int16_t>& left, const std::vector<int16_t>& right);

    // Generates an id to send with events and messages
    static std::string generateId(const std::string& prefix, size_t length = 21);

    // Function to retrieve the OpenAI API key from the environment variables
    static bool get_openai_key(std::string& res);

    // Function to retrieve the OpenAI base URL from the environment variables
    static bool get_openai_baseurl(std::string& res);

private:
    // Helper functions for base64 encoding and decoding
    static std::string base64_encode(const std::string &in);
    static std::string base64_decode(const std::string &in);
};

#endif // UTILS_H
