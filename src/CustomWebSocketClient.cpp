#include "CustomWebSocketClient.h"
#include "Utils.h"


CustomWebSocketClient::CustomWebSocketClient() : WebSocketClient() {
    setListeningPort(PORT);
    setRawAudio(true);
}

void CustomWebSocketClient::createConversationitem(std::string &message)
{
    json sendMsg;
    sendMsg["send"] = message;
    sendMessage(sendMsg.dump());
}

void CustomWebSocketClient::createResponse()
{
}

void CustomWebSocketClient::cancelAssistantSpeech()
{
}

void CustomWebSocketClient::onMessage(std::string& message) {
    try {
        std::string eventType;
        // Parse the JSON message
        json eventJson = json::parse(message);
        eventType = eventJson["type"];
        std::cout << "event: " << eventType << std::endl;

        if (eventType == "audio") {
            std::string audioData = eventJson["data"];
            auto deltaPart = std::make_shared<ItemContentDeltaType>();
            Utils::base64DecodeAudio(audioData, deltaPart->audio);
            onAudioDelta(deltaPart);
        };

    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error processing event: " << e.what() << std::endl;
    }
}