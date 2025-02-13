#ifndef CONVERSATION_H
#define CONVERSATION_H

#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include <functional>
#include <chrono>
#include <stdexcept>
#include <nlohmann/json.hpp>

struct Event {
    std::string type;
    nlohmann::json data;
};

struct Response {
    std::string id;
    std::vector<std::string> output;
};

struct ItemContentDeltaType {
    std::string text;
    std::vector<int16_t> audio;
    std::string arguments;
    std::string transcript;
};

struct Formatted {
    std::string text;
    std::string transcript;
    std::vector<int16_t> audio;
    std::string arguments;
};
// Define the Item structure
struct ItemType {
    std::string id;
    std::string type;
    std::string role;
    std::string status;
    std::vector<std::shared_ptr<ItemContentDeltaType>>content;
    std::string call_id;
    std::string arguments;
    std::string output;
    Formatted formatted;
    std::chrono::steady_clock::time_point time;
};

class Conversation {
public:
    Conversation(); 
    ~Conversation() = default;
    void clear();

    // Public Methods
    std::pair<std::shared_ptr<ItemType>, std::shared_ptr<ItemContentDeltaType>> processEvent(const Event& event); 
    bool registerCallback(const std::string& type, std::function<std::pair<std::shared_ptr<ItemType>, std::shared_ptr<ItemContentDeltaType>>(const Event&)> callback);

    std::shared_ptr<ItemType> getItem(const std::string& id);
    std::shared_ptr<ItemType> getRecentAssistantMessage() {
        auto it = std::find_if(items.rbegin(), items.rend(),
            [](const auto& item) { return item->role == "assistant"; });
        return it != items.rend() ? *it : nullptr;
    }
    int frequency = 8000;
    int isTalking = 0;
private:
    // Private Members
    std::map<std::string, std::shared_ptr<ItemType>> itemLookup;
    std::vector<std::shared_ptr<ItemType>> items;
    std::map<std::string, std::shared_ptr<Response>> responseLookup;
    std::vector<std::shared_ptr<Response>> responses;

    // Event Processors
    std::unordered_map<std::string, std::function<std::pair<std::shared_ptr<ItemType>, std::shared_ptr<ItemContentDeltaType>>(const Event&)>> eventProcessors; 

    // Initialize Event Processors
    void initializeEventProcessors();
};

#endif // CONVERSATION_H
