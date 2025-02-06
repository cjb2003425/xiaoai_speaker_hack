#ifndef REALTIMECLIENT_H
#define REALTIMECLIENT_H

#include <string>
#include <nlohmann/json.hpp>

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
    bool create_conversation_item(std::string& message, std::string& result);
    bool create_response(std::string& result);

protected:
    json session;
    bool quitRequest;
    // Additional shared methods can be added here
};

#endif