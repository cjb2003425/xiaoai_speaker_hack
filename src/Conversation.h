#ifndef CONVERSATION_H
#define CONVERSATION_H

#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include <functional>
#include <stdexcept>
#include <nlohmann/json.hpp>

struct Event {
    std::string event_id;
    std::string type;
    nlohmann::json data;
};

struct Response {
    std::string id;
    std::vector<std::string> output;
};

struct ItemContentDeltaType {
    std::string text;
    std::vector<std::string> audio;
    std::string arguments;
    std::string transcript;
};

struct Formatted {
    std::string text;
    std::string transcript;
    std::string audio;
    std::string arguments;
};
// Define the Item structure
struct ItemType {
    std::string id;
    std::string type;
    std::string role;
    std::string status;
    std::vector<ItemContentDeltaType>content;
    std::string name;
    std::string call_id;
    std::string arguments;
    std::string output;
    Formatted formatted;
};

class Conversation {
public:
    Conversation() {
        initializeEventProcessors();
    }
    ~Conversation() = default;
    void clear();

    // Public Methods
    std::pair<ItemType*, ItemContentDeltaType*> processEvent(const Event& event); 

    ItemType* getItem(const std::string& id);
    std::vector<ItemType> getItems() const;


private:
    // Private Members
    std::map<std::string, ItemType> itemLookup;
    std::vector<ItemType> items;
    std::map<std::string, Response> responseLookup;
    std::vector<Response> responses;
    std::map<std::string, std::map<std::string, std::string>> queuedSpeechItems;
    std::map<std::string, std::map<std::string, std::string>> queuedTranscriptItems;

    // Event Processors
    std::unordered_map<std::string, std::function<std::pair<ItemType*, ItemContentDeltaType*>(const Event&)>> EventProcessors; 

    // Initialize Event Processors
    void initializeEventProcessors();
};

#endif // CONVERSATION_H
