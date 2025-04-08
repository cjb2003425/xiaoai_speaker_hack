#pragma once

#include "WebSocketClient.h"
#include <functional>

class CustomWebSocketClient : public WebSocketClient {
public:
    CustomWebSocketClient();
    ~CustomWebSocketClient() override = default;
    void createConversationitem(std::string& message) override;
    void createResponse() override;
    void cancelAssistantSpeech() override;

protected:
    void onMessage(std::string& message) override;
    

private:
    static constexpr int PORT = 8765;
    bool rawAudio = true;
};