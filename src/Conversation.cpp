#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>
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
}

std::pair<std::shared_ptr<ItemType>, std::shared_ptr<ItemContentDeltaType>> Conversation::processEvent(const Event& event) {
    if (event.type.empty()) {
        std::cerr << "Missing \"type\" on event" << std::endl;
        throw std::invalid_argument("Missing \"type\" on event");
    }
    auto it = eventProcessors.find(event.type);
    if (it == eventProcessors.end()) {
        std::cerr << "Event type \"" << event.type << "\" not registered" << std::endl;
        return std::make_pair(nullptr, nullptr);
    }
    return it->second(event);
}

std::shared_ptr<ItemType> Conversation::getItem(const std::string& id) {
    auto it = itemLookup.find(id);
    return it != itemLookup.end() ? it->second : nullptr;
}

Conversation::Conversation() {
    initializeEventProcessors();
}

void Conversation::initializeEventProcessors() {
    eventProcessors = {
        {"conversation.item.created", [this](const Event& event) {
            std::cout << "conversation.item.created" << std::endl;
            const auto& item = event.data.at("item");
            std::shared_ptr<ItemType> newItem = std::make_shared<ItemType>();
            newItem->id = item.at("id");
            newItem->formatted.text = "";
            newItem->formatted.transcript = "";
            newItem->status = item.at("status");
            newItem->role = item.at("role");
            newItem->type = item.at("type");

            auto it = itemLookup.find(newItem->id);
            if (it == itemLookup.end()) {
                itemLookup[newItem->id] = newItem;
                items.push_back(newItem);
            }

            if (newItem->role == "assistant") {
                newItem->time = std::chrono::steady_clock::now();
            }
            std::cout << "add item:" << newItem->id << std::endl;
            return std::make_pair(newItem, nullptr);
        }},
        {"conversation.item.truncated", [this](const Event& event) {
            std::cout << "conversation.item.truncated" << std::endl;
            const std::string& item_id = event.data.at("item_id");
            int audio_end_ms = event.data.at("audio_end_ms").get<int>();
            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::invalid_argument("item.truncated: Item \"" + item_id + "\" not found");
            }
            std::shared_ptr<ItemType> foundItem = it->second;
            int endIndex = (audio_end_ms * frequency) / 1000;
            foundItem->formatted.transcript = "";
            foundItem->formatted.audio.resize(endIndex);
            return std::make_pair(foundItem, nullptr);
        }},
        {"conversation.item.deleted", [this](const Event& event) {
            std::cout << "conversation.item.deleted" << std::endl;
            const std::string& item_id = event.data.at("item_id");
            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::invalid_argument("item.deleted: Item \"" + item_id + "\" not found");
            }
            std::shared_ptr<ItemType> foundItem = it->second;
            itemLookup.erase(it);
            auto vecIt = std::find_if(items.begin(), items.end(), [&item_id](const std::shared_ptr<ItemType>& i) { return i->id == item_id; });
            if (vecIt != items.end()) {
                items.erase(vecIt);
            }
            return std::make_pair(foundItem, nullptr);
        }},
        {"conversation.item.input_audio_transcription.completed", [this](const Event& event) {
            return std::make_pair(nullptr, nullptr);
        }},
        {"input_audio_buffer.speech_started", [this](const Event& event) {
            return std::make_pair(nullptr, nullptr);
        }},
        {"input_audio_buffer.speech_stoppted", [this](const Event& event) {
            return std::make_pair(nullptr, nullptr);
        }},
        {"response.created", [this](const Event& event) {
            std::cout << "response.created" << std::endl;
            std::shared_ptr<Response> response = std::make_shared<Response>();
            const auto& newResponse = event.data.at("response");
            response->id = newResponse.at("id");

            if (responseLookup.find(response->id) == responseLookup.end()) {
                responseLookup[response->id] = response;
                responses.push_back(response);
            }
            return std::make_pair(nullptr, nullptr);
        }},
        {"response.output_item.added", [this](const Event& event) {
            std::cout << "response.output_item.added" << std::endl;
            std::string response_id = event.data.at("response_id");
            const auto& item = event.data.at("item");
            const std::string& item_id = item.at("id");

            auto it = responseLookup.find(response_id);
            if (it == responseLookup.end()) {
                throw std::runtime_error("response.output_item.added: Response '" + response_id + "' not found");
            }
            std::shared_ptr<Response> response = it->second;
            response->output.push_back(item_id);

            return std::make_pair(nullptr, nullptr);
        }},
        {"response.output_item.done", [this](const Event& event) {
            std::cout << "response.output_item.done" << std::endl;
            const auto& item = event.data.at("item");
            const std::string& item_id = item.at("id");
            const std::string& status = item.at("status");

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.output_item.done: Item '" + item_id + "' not found");
            }

            std::shared_ptr<ItemType> found = it->second;
            found->status = status;
            return std::make_pair(found, nullptr);
        }},
        {"response.content_part.added", [this](const Event& event) {
            std::cout << "response.content_part.added" << std::endl;
            std::string item_id = event.data.at("item_id");
            const auto& part = event.data.at("part");
            std::shared_ptr<ItemContentDeltaType> content = std::make_shared<ItemContentDeltaType>();

            if (part.find("text") != part.end()) {
                content->text = part.at("text");
            } else if (part.find("audio") != part.end()) {
            }

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.content_part.added: Item '" + item_id + "' not found");
            }

            std::shared_ptr<ItemType> item = it->second;
            item->content.push_back(content);

            return std::make_pair(item, nullptr);

        }},
        {"response.audio_transcript.delta", [this](const Event& event) {
            std::cout << "response.audio_transcript.delta" << std::endl;
            std::string item_id = event.data.at("item_id");
            int content_index = event.data.at("content_index").get<int>();
            std::string delta = event.data.at("delta");

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.audio_transcript.delta: Item '" + item_id + "' not found");
            }

            std::shared_ptr<ItemType> item = it->second;
            item->content[content_index]->transcript += delta;
            item->formatted.transcript += delta;

            auto deltaPart = std::make_shared<ItemContentDeltaType>();
            deltaPart->transcript = delta;
            return std::make_pair(item, deltaPart);
        }},
        {"response.audio.delta", [this](const Event& event) {
            std::cout << "response.audio.delta" << std::endl;
            std::string item_id = event.data.at("item_id");
            int content_index = event.data.at("content_index").get<int>();
            std::string delta = event.data.at("delta");

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.audio_transcript.delta: Item '" + item_id + "' not found");
            }

            std::shared_ptr<ItemType> item = it->second;
            // Convert base64 to array buffer and then to Int16Array
            std::vector<unsigned char> buffer = Utils::base64ToArrayBuffer(delta);
            std::vector<int16_t> appendValues(buffer.begin(), buffer.end());
            
            // Merge arrays
            std::vector<int16_t> mergedAudio = Utils::mergeInt16Arrays(
                item->formatted.audio,
                appendValues
            );
            item->formatted.audio = mergedAudio;
            auto deltaPart = std::make_shared<ItemContentDeltaType>();
            deltaPart->audio = appendValues;
            return std::make_pair(item, deltaPart);
        }},
        {"response.text.delta", [this](const Event& event) {
            std::cout << "response.text.delta" << std::endl;
            std::string item_id = event.data.at("item_id");
            int content_index = event.data.at("content_index").get<int>();
            std::string delta = event.data.at("delta");

            auto it = itemLookup.find(item_id);
            if (it == itemLookup.end()) {
                throw std::runtime_error("response.audio_transcript.delta: Item '" + item_id + "' not found");
            }

            std::shared_ptr<ItemType> item = it->second;
            item->content[content_index]->text += delta;
            item->formatted.text += delta;
            
            auto deltaPart = std::make_shared<ItemContentDeltaType>();
            deltaPart->text = delta;
            return std::make_pair(item, deltaPart);
        }},
        {"response.function_call_arguments.delta", [this](const Event& event) {
            std::cout << "response.function_call_arguments.delta" << std::endl;
            return std::make_pair(nullptr, nullptr);
        }},
        {"response.audio.done", [this](const Event& event) {
            std::cout << "response.audio.done" << std::endl;
            return std::make_pair(nullptr, nullptr);
        }},
        {"response.audio_transcript.done", [this](const Event& event) {
            std::cout << "response.audio_transcript.done" << std::endl;
            return std::make_pair(nullptr, nullptr);
        }},
        {"response.content_part.done", [this](const Event& event) {
            std::cout << "response.content_part.done" << std::endl;
            return std::make_pair(nullptr, nullptr);
        }},
        {"output_audio_buffer.started", [this](const Event& event) {
            std::cout << "response.audio.started" << std::endl;
            isTalking = 1;
            return std::make_pair(nullptr, nullptr);
        }},
        {"output_audio_buffer.stopped", [this](const Event& event) {
            std::cout << "response.audio.stopped" << std::endl;
            isTalking = 0;
            return std::make_pair(nullptr, nullptr);
        }},
        {"response.done", [this](const Event& event) {
            std::cout << "response.done" << std::endl;
            const auto& response = event.data.at("response");
            const auto& usage = response.at("usage");

            std::cout << "total token:" << usage.at("total_tokens") << std::endl;
            return std::make_pair(nullptr, nullptr);
        }},
        {"error", [this](const Event& event) {
            auto error = event.data.at("error");
            std::cerr << "Error: " << error.at("message") << std::endl;
            return std::make_pair(nullptr, nullptr);
        }}
    };
}

bool Conversation::registerCallback(const std::string& type, std::function<std::pair<std::shared_ptr<ItemType>, std::shared_ptr<ItemContentDeltaType>>(const Event&)> callback) {
    auto it = eventProcessors.find(type);
    if (it != eventProcessors.end()) {
        std::cerr << "Callback for event type \"" << type << "\" already registered." << std::endl;
        return false;
    }
    eventProcessors[type] = callback;
    return true;
}
