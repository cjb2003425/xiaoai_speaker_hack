#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <functional>
#include <ctime>
#include <algorithm> 
#include "Conversation.h"
#include "Utils.h"

void Conversation::clear() {
    itemLookup.clear();
    items.clear();
    responseLookup.clear();
    responses.clear();
    queuedTranscriptItems.clear();
}

std::pair<ItemType*, ItemContentDeltaType*> Conversation::processEvent(const Event& event) {
    if (event.type.empty()) {
        std::cerr << "Missing \"type\" on event" << std::endl;
        throw std::invalid_argument("Missing \"type\" on event");
    }
    auto it = EventProcessors.find(event.type);
    if (it == EventProcessors.end()) {
        return std::make_pair(nullptr, nullptr);
    }
    std::cout << "event: " << event.type << std::endl;
    return it->second(event);
}

ItemType* Conversation::getItem(const std::string& id) {
    auto it = itemLookup.find(id);
    return it != itemLookup.end() ? &it->second : nullptr;
}

std::vector<ItemType> Conversation::getItems() const {
    return items;
}

void Conversation::initializeEventProcessors() {
    std::unordered_map<std::string, std::function<std::pair<ItemType*, ItemContentDeltaType*>(const Event&)>> EventProcessors = {
        {"conversation.item.created", [this](const Event& event) {
            const auto& item = event.data.at("item");
            ItemType newItem;
            newItem.id = item.at("id");
            newItem.formatted.text = "";
            newItem.formatted.transcript = "";

            if (item.find("content") != item.end()) {
                // Populate formatted text if it comes out on creation
                if (item.at("type") == "text" || item.at("type") == "input_text") {
                    newItem.formatted.text += item.at("text");
                }
            }

            if (queuedTranscriptItems.find(newItem.id) != queuedTranscriptItems.end()) {
                newItem.formatted.transcript = queuedTranscriptItems[newItem.id]["transcript"];
                queuedTranscriptItems.erase(newItem.id);
            }

            if (newItem.type == "message") {
                if (newItem.role == "user") {
                    newItem.status = "completed";
                } else {
                    newItem.status = "in_progress";
                }
            } else if (newItem.type == "function_call") {
                newItem.formatted.arguments = item.at("arguments");
                newItem.status = "in_progress";
            } else if (newItem.type == "function_call_output") {
                newItem.status = "completed";
                newItem.formatted.arguments = item.at("output");
            }

            itemLookup[newItem.id] = newItem;
            items.push_back(newItem);

            return std::make_pair(&itemLookup[newItem.id], nullptr);
        }},
        {"conversation.item.truncated", [this](const Event& event) {
            const std::string& item_id = event.data.at("item_id");
            int audio_end_ms = std::stoi(event.data.at("audio_end_ms").get<std::string>());
            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::invalid_argument("item.truncated: Item \"" + item_id + "\" not found");
            }
            ItemType& foundItem = it->second;
            int endIndex = (audio_end_ms * frequence) / 1000;
            foundItem.formatted.transcript = "";
            foundItem.formatted.audio.resize(endIndex);
            return std::make_pair(&foundItem, nullptr);
        }},
        {"conversation.item.deleted", [this](const Event& event) {
            const std::string& item_id = event.data.at("item_id");
            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::invalid_argument("item.deleted: Item \"" + item_id + "\" not found");
            }
            ItemType foundItem = it->second;
            itemLookup.erase(it);
            auto vecIt = std::find_if(items.begin(), items.end(), [&item_id](const ItemType& i) { return i.id == item_id; });
            if (vecIt != items.end()) {
                items.erase(vecIt);
            }
            return std::make_pair(&foundItem, nullptr);
        }},
        {"conversation.item.input_audio_transcription.completed", [this](const Event& event) {
            const std::string& item_id = event.data.at("item_id");
            int content_index = std::stoi(event.data.at("content_index").get<std::string>());
            std::string transcript = event.data.at("transcript");
            std::string formattedTranscript = transcript.empty() ? " " : transcript;
            auto it = itemLookup.find(item_id);

            if (it == itemLookup.end()) {
                throw std::invalid_argument("item.deleted: Item \"" + item_id + "\" not found");
            }
            ItemType foundItem = it->second;
            foundItem.content[content_index].transcript = transcript;
            foundItem.formatted.transcript = formattedTranscript;

            auto* delta = new ItemContentDeltaType();
            delta->transcript = transcript;
            return std::make_pair(&foundItem, delta);
        }},
        {"input_audio_buffer.speech_started", [this](const Event& event) {
            return std::make_pair(nullptr, nullptr);
        }},
        {"input_audio_buffer.speech_stoppted", [this](const Event& event) {
            return std::make_pair(nullptr, nullptr);
        }},
        {"response.created", [this](const Event& event) {
            Response response;
            const auto& newResponse = event.data.at("response");
            response.id = newResponse.at("id");

            if (responseLookup.find(response.id) == responseLookup.end()) {
                responseLookup[response.id] = response;
                responses.push_back(response);
            }
            return std::make_pair(nullptr, nullptr);
        }},
        {"response.output_item.added", [this](const Event& event) {
            std::string response_id = event.data.at("response_id");
            const auto& item = event.data.at("item");
            const std::string& item_id = item.at("id");

            auto it = responseLookup.find(response_id);
            if (it == responseLookup.end()) {
                throw std::runtime_error("response.output_item.added: Response '" + response_id + "' not found");
            }

            Response& response = it->second;
            response.output.push_back(item_id);
            return std::make_pair(nullptr, nullptr);
        }},
        {"response.output_item.done", [this](const Event& event) {
            const auto& item = event.data.at("item");
            const std::string& item_id = item.at("id");
            const std::string& status = item.at("status");

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.output_item.done: Item '" + item_id + "' not found");
            }

            ItemType& found = it->second;
            found.status = status;
            return std::make_pair(&found, nullptr);
        }},
        {"response.content_part.added", [this](const Event& event) {
            std::string item_id = event.data.at("item_id");
            const auto& part = event.data.at("part");
            ItemContentDeltaType content;

            if (part.find("text") != part.end()) {
                content.text = part.at("text");
            } else if (part.find("audio") != part.end()) {
            }

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.content_part.added: Item '" + item_id + "' not found");
            }

            ItemType& item = it->second;
            item.content.push_back(content);

            return std::make_pair(&item, nullptr);

        }},
        {"response.audio_transcript.delta", [this](const Event& event) {
            std::string item_id = event.data.at("item_id");
            int content_index = std::stoi(event.data.at("content_index").get<std::string>());
            std::string delta = event.data.at("delta");

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.audio_transcript.delta: Item '" + item_id + "' not found");
            }

            ItemType& item = it->second;
            item.content[content_index].transcript += delta;
            item.formatted.transcript += delta;

            auto* deltaPart = new ItemContentDeltaType();
            deltaPart->transcript = delta;
            return std::make_pair(&item, deltaPart);
        }},
        {"response.audio.delta", [this](const Event& event) {
            std::string item_id = event.data.at("item_id");
            int content_index = std::stoi(event.data.at("content_index").get<std::string>());
            std::string delta = event.data.at("delta");

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.audio_transcript.delta: Item '" + item_id + "' not found");
            }

            ItemType& item = it->second;
            // Convert base64 to array buffer and then to Int16Array
            std::vector<unsigned char> buffer = Utils::base64ToArrayBuffer(delta);
            std::vector<int16_t> appendValues(buffer.begin(), buffer.end());
            
            // Merge arrays
            std::vector<int16_t> mergedAudio = Utils::mergeInt16Arrays(
                item.formatted.audio,
                appendValues
            );
            item.formatted.audio = mergedAudio;
            auto* deltaPart = new ItemContentDeltaType();
            deltaPart->audio = appendValues;
            return std::make_pair(&item, deltaPart);
        }},
        {"response.text.delta", [this](const Event& event) {
            std::string item_id = event.data.at("item_id");
            int content_index = std::stoi(event.data.at("content_index").get<std::string>());
            std::string delta = event.data.at("delta");

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.audio_transcript.delta: Item '" + item_id + "' not found");
            }

            ItemType& item = it->second;
            item.content[content_index].text += delta;
            item.formatted.text += delta;
            
            auto* deltaPart = new ItemContentDeltaType();
            deltaPart->text = delta;
            return std::make_pair(&item, deltaPart);
        }},
        {"response.function_call_arguments.delta", [this](const Event& event) {
            return std::make_pair(nullptr, nullptr);
        }}
    };

}
