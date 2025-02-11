#ifndef REALTIMECLIENT_H
#define REALTIMECLIENT_H

#include <string>
#include <nlohmann/json.hpp>
#include "Conversation.h"

// For convenience
using json = nlohmann::json;

class RealTimeClient {
public:
    RealTimeClient();
    virtual ~RealTimeClient();
    
    virtual bool loop() = 0;
    virtual bool init() = 0;
    virtual bool quit();
    virtual bool sendMessage(const std::string& message) = 0;
    
    virtual void onMessage(std::string& message);
    void createConversationitem(std::string& message);
    void createResponse();
    void updateSession(const std::string& message);
    void cancelResponse();

protected:
    json session;
    bool quitRequest;
    bool retryRequest;
    bool talking;
    bool mute;
    Conversation conversation;

    // Additional shared methods can be added here
};

#endif