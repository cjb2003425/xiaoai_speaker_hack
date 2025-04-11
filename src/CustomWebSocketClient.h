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
    void sendHelloMessage();
    void onClientEstablished() override;
    void onClientClosed() override;
    void initMediaPlayback();

protected:
    void onMessage(std::string& message) override;
    

private:
    static constexpr int PORT = 8000;
    static constexpr int OPUS_FRAME_DURATION_MS = 60;
    bool rawAudio = false;
    int sample_rate = 0;
    int frame_duration = 20; // in ms
    std::string session_id;
};