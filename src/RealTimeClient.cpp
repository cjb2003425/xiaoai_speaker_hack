#include <iostream>
#include <chrono>
#include <ctime>
#include "RealTimeClient.h"


// JSON template constants
const std::string CONVERSATION_ITEM_CREATE_TEMPLATE = R"(
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

RealTimeClient::RealTimeClient() {
    conversation.registerCallback("session.created", [this](const Event& event) {
            session = event.data;
            std::cout << "session created" << std::endl;
            updateSession(SESSION_UPDATE_TEMPLATE);
            return std::make_pair(nullptr, nullptr);
        });

    conversation.registerCallback("session.updated", [this](const Event& event) {
            session = event.data;
            std::cout << "session updated" << std::endl;
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
        std::pair<std::shared_ptr<ItemType>, std::shared_ptr<ItemContentDeltaType>> ret = conversation.processEvent(event);
        if (event.type == "response.audio.delta") {
            onAudioDelta(ret.second);
        } else if (event.type == "response.audio.done") {
            onAudioDone(ret.first);
        };

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

    //clearOlderItems();
    try {
        json item = json::parse(CONVERSATION_ITEM_CREATE_TEMPLATE);
        item["item"]["content"][0]["text"] = message;
        sendMessage(item.dump());
    } catch (const json::parse_error& e) {
        std::cout << "Failed to parse conversation item JSON: " + std::string(e.what());
    }
}

void RealTimeClient::createResponse() {
    sendMessage(RESPONSE_CREATE_TEMPLATE);
    wakeupOn = false;
}

void RealTimeClient::cancelAssistantSpeech() {

    std::shared_ptr<ItemType> recent = conversation.getRecentAssistantMessage();
    if (!recent) {
        std::cerr << "can't cancel, no recent assistant message found" << std::endl;
        return;
    }

    if (recent->status == "completed") {
        std::cerr << "No truncation needed, message is completed" << std::endl;
        clearOutputBuffer();
        return;
    }

    try {
        json cancelItem;
        cancelItem["type"] = "response.cancel";
        sendMessage(cancelItem.dump());

        auto now = std::chrono::steady_clock::now();
        auto elapse = std::chrono::duration_cast<std::chrono::milliseconds>(now - recent->time);
        json item;
        item["type"] = "conversation.item.truncate";
        item["content_index"] = 0;
        item["item_id"] = recent->id;
        item["audio_end_ms"] = elapse.count();
        std::cout << "elpse:" << elapse.count() << std::endl;
        sendMessage(item.dump());
    } catch (const json::parse_error& e) {
        std::cerr << "Failed to parse conversation item JSON: " + std::string(e.what());
    }
    clearOutputBuffer();
}

void RealTimeClient::updateSession(const std::string& msg) {
    try {
        json item = json::parse(msg);
        sendMessage(item.dump());
    } catch (const json::parse_error& e) {
        std::cout << "Failed to parse conversation item JSON: " + std::string(e.what());
    }
}

void RealTimeClient::clearOlderItems() {
    int size = conversation.getItemSize();
    while (size > KEEP_NUM) {
        std::shared_ptr<ItemType> item = conversation.popFrontItem();
        if (item) {
            std::cout << "clear item:" << item->id << std::endl;
            json itemJson;
            itemJson["type"] = "conversation.item.delete";
            itemJson["item_id"] = item->id;
            sendMessage(itemJson.dump());
        } else {
            std::cout << "no item to clear" << std::endl;
        }
        size--;
    }
}