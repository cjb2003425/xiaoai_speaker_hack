#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <utility>


class AudioBuffer;
class Utils {
public:
    static void base64DecodeAudio(const std::string &input, AudioBuffer& buffer); 
    // Generates an id to send with events and messages
    static std::string generateId(const std::string& prefix, size_t length = 21);

    // Function to retrieve the OpenAI API key from the environment variables
    static bool get_openai_key(std::string& res);

    // Function to retrieve the OpenAI base URL from the environment variables
    static bool get_openai_baseurl(std::string& res);

private:
    static std::pair<std::unique_ptr<uint8_t[]>, size_t> base64_decode(const std::string& in); 
};

#endif // UTILS_H
