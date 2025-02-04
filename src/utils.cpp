#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>

using json = nlohmann::json;

// JSON template constants
const std::string CONVERSATION_ITEM_TEMPLATE = R"(
{
    "type": "conversation.item.create",
    "item": {
        "type": "message",
        "role": "user",
        "content": [
            {
                "type": "input_text"
            }
        ]
    }
}
)";

const std::string RESPONSE_TEMPLATE = R"({
    "type": "response.create",
    "response": {
        "modalities": [ "text", "audio"]
    }
})";

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

/// @brief Creates a conversation item JSON message
/// @param message The message text to include
/// @param[out] result Reference to store the resulting JSON string
/// @return true if JSON was created successfully, false otherwise
/// @throws std::invalid_argument if message is empty
/// @throws std::runtime_error if JSON parsing fails
bool create_conversation_item(std::string& message, std::string& result) {
    if (message.empty()) {
        throw std::invalid_argument("Message cannot be empty");
    }

    try {
        json item = json::parse(CONVERSATION_ITEM_TEMPLATE);
        item["item"]["content"][0]["text"] = message;
        result = item.dump();
    } catch (const json::parse_error& e) {
        std::cout << "Failed to parse conversation item JSON: " + std::string(e.what());
        return false;
    }
    return true;
}

/// @brief Creates a response JSON message
/// @param[out] result Reference to store the resulting JSON string
/// @return true if JSON was created successfully
bool create_response(std::string& result) {
    result = RESPONSE_TEMPLATE;
    return true;
}
