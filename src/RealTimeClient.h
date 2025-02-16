#ifndef REALTIMECLIENT_H
#define REALTIMECLIENT_H

#include <string>
#include <nlohmann/json.hpp>
#include "Conversation.h"

// For convenience
using json = nlohmann::json;

class RealTimeClient {
public:
    static constexpr int KEEP_NUM = 3; 
    RealTimeClient();
    virtual ~RealTimeClient();
    
    virtual bool loop() = 0;
    virtual bool init() = 0;
    virtual bool quit();
    virtual bool sendMessage(const std::string& message) = 0;
    
    virtual void onMessage(std::string& message);
    virtual void onAudioDelta(std::shared_ptr<ItemContentDeltaType> delta) {};
    virtual void onAudioDone(std::shared_ptr<ItemType> item) {};
    void createConversationitem(std::string& message);
    void clearOlderItems();
    void createResponse();
    void updateSession(const std::string& message);
    void cancelAssistantSpeech();
    void setFrequency(int frequency) {
        conversation.frequency = frequency;
    };
    void setWakeupOn(bool wakeup) {
        wakeupOn = wakeup;
        if (wakeup) {
            cancelAssistantSpeech();
        }
    };
    void clear() {
        conversation.clear();
    }

protected:
    json session;
    bool quitRequest = false;
    bool retryRequest = false;
    bool wakeupOn = false;
    Conversation conversation;

    // Additional shared methods can be added here
};

#endif