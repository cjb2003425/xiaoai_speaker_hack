#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>



/// @brief Retrieves OpenAI API key from environment variables
/// @param[out] res Reference to store the API key
/// @return true if key was found and stored successfully, false otherwise
/// @throws std::runtime_error if environment variable access fails
bool get_openai_key(std::string& res) {
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
bool get_openai_baseurl(std::string& res) {
    const char* base_url = getenv("OPENAI_REALTIMEAPI");
    if (!base_url || std::string(base_url).empty()) {
        fprintf(stderr, "[Error]: OPENAI_REALTIMEAPI not set or empty in environment.\n");
        return false;
    }
    res.assign(base_url);
    return true;
}
