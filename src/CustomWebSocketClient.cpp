#include "CustomWebSocketClient.h"
#include "Utils.h"
#include "main.h"

CustomWebSocketClient::CustomWebSocketClient() : WebSocketClient() {
    setListeningPort(PORT);
    setRawAudio(false);
}

void CustomWebSocketClient::createConversationitem(std::string &message)
{
    std::string json = "{\"session_id\":\"" + session_id + 
        "\",\"type\":\"listen\",\"state\":\"detect\",\"text\":\"" + message + "\"}";
    sendMessage(json);
}

void CustomWebSocketClient::createResponse()
{
}

void CustomWebSocketClient::cancelAssistantSpeech()
{
}

void CustomWebSocketClient::sendHelloMessage()
{
    // Send hello message to describe the client
    // keys: message type, version, audio_params (format, sample_rate, channels)
    std::string message = "{";
    message += "\"type\":\"hello\",";
    message += "\"version\": 1,";
    message += "\"transport\":\"websocket\",";
    message += "\"audio_params\":{";
    message += "\"format\":\"opus\", \"sample_rate\":24000, \"channels\":1, \"frame_duration\":" + std::to_string(OPUS_FRAME_DURATION_MS);
    message += "}}";
    sendMessage(message);
}

void CustomWebSocketClient::onClientEstablished()
{
    std::cout << "Client established" << std::endl;
    sendHelloMessage();
}

void CustomWebSocketClient::onClientClosed()
{
    std::cout << "Client closed" << std::endl;
}

void CustomWebSocketClient::initMediaPlayback()
{ 
    oai_init_audio_alsa(sample_rate);
    oai_init_audio_decoder(sample_rate);
}

void CustomWebSocketClient::onMessage(std::string& message) {
    try {
        std::string eventType;
        // Parse the JSON message
        json eventJson = json::parse(message);
        eventType = eventJson["type"];

        std::cout << "receive: " << message << std::endl;
        if (eventType == "hello") {
            session_id = eventJson["session_id"];
            sample_rate = eventJson["audio_params"]["sample_rate"];
            frame_duration = eventJson["audio_params"]["frame_duration"];
            initMediaPlayback();
        } else if (eventType == "event") {
            std::cout << "status: " << eventJson["status"] << std::endl;
        }

    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error processing event: " << e.what() << std::endl;
    }
}