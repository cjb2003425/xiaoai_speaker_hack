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

const std::string SESSION_UPDATE_TEMPLATE = R"({
    "type": "session.update",
    "session": {
        "modalities": ["text", "audio"],
        "voice": "sage",
        "input_audio_format": "pcm16",
        "output_audio_format": "pcm16",
        "input_audio_transcription":null,
        "turn_detection":null,
        "tools": [],
        "tool_choice": "auto",
        "temperature": 0.8,
        "max_response_output_tokens": "inf"
    }
})";

const std::string RESPONSE_CREATE_TEMPLATE = R"({
    "type": "response.create",
    "response": {
        "modalities": [ "text", "audio"]
    }
})";

const std::string RESPONSE_CANCEL_TEMPLATE = R"({
    "type": "response.cancel"
})";

RealTimeClient::RealTimeClient() : 
    quitRequest(false), 
    retryRequest(false),
    talking(false),
    mute(false) {
    conversation.registerCallback("session.created", [this](const Event& event) {
            session = event.data;
            std::cout << "session created" << std::endl;
            updateSession(SESSION_UPDATE_TEMPLATE);
            return std::make_pair(nullptr, nullptr);
        });
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
        Event event;
        // Parse the JSON message
        json eventJson = json::parse(message);
        event.type = eventJson["type"];
        event.data = eventJson;
        std::cout << "event: " << event.type << std::endl;
        std::cout << "data" << event.data.dump() << std::endl;

        // Process the event
        conversation.processEvent(event);
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error processing event: " << e.what() << std::endl;
    }
}

void RealTimeClient::createConversationitem(std::string& message) {
    if (message.empty()) {
        return;
    }

    try {
        json item = json::parse(CONVERSATION_ITEM_TEMPLATE);
        item["item"]["content"][0]["text"] = message;
        sendMessage(item.dump());
    } catch (const json::parse_error& e) {
        std::cout << "Failed to parse conversation item JSON: " + std::string(e.what());
    }
}

void RealTimeClient::createResponse() {
    sendMessage(RESPONSE_CREATE_TEMPLATE);
}

void RealTimeClient::cancelResponse() {
    if (talking) {
        std::cout << "Cancel response" << std::endl;
        mute  = true;
        sendMessage(RESPONSE_CANCEL_TEMPLATE);
    }
}

void RealTimeClient::updateSession(const std::string& msg) {
    try {
        json item = json::parse(msg);
        sendMessage(item.dump());
    } catch (const json::parse_error& e) {
        std::cout << "Failed to parse conversation item JSON: " + std::string(e.what());
    }
}
