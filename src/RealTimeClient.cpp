#include <iostream>
#include "RealTimeClient.h"


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

RealTimeClient::RealTimeClient() : quitRequest(false) {
    // Constructor implementation
}

RealTimeClient::~RealTimeClient() {
    // Destructor implementation
}

bool RealTimeClient::quit() {
    quitRequest = true;
    return quitRequest;
}

void RealTimeClient::onMessage(std::string& message) {
    try {
        // Parse the JSON message
        json event = json::parse(message);
      	std::cout << "message type is " << event["type"] << std::endl;
        if (event["type"] == "session.created") {
            session = event["session"];
            std::cout << message << std::endl;
        } else if (event["type"] == "session.updated") {
            session = event["session"];
        } else if (event["type"] == "conversation.item.created") {
        } else if (event["type"] == "response.audio_transcript.delta") {
            //std::cout << event["delta"] << std::endl;
        } else if (event["type"] == "response.audio_transcript.done") {
            std::cout << event["transcript"] << std::endl;
        } else if (event["type"] == "response.done") {
            std::cout << event["response"]["usage"] << std::endl;
        } else if (event["type"] == "error") {
            std::cout << message << std::endl;
        } else {
            //std::cout << json_data << std::endl;
        }
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }

}

bool RealTimeClient::create_conversation_item(std::string& message, std::string& result) {
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

bool RealTimeClient::create_response(std::string& result) {
    result = RESPONSE_TEMPLATE;
    return true;
}