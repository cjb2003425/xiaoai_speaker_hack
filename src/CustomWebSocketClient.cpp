#include <chrono>
#include <iomanip>  // Add this line
#include "Utils.h"
#include "CustomWebSocketClient.h"
#include "main.h"

CustomWebSocketClient::CustomWebSocketClient() : WebSocketClient() {
    setListeningPort(PORT);
    setRawAudio(false);
}

void CustomWebSocketClient::createConversationitem(std::string &message)
{
    std::string jsonMessage = R"(
    {
        "type": "listen",
        "state": "detect"
    })";

    try {
        json item = json::parse(jsonMessage);
        item["session_id"] = session_id;
        item["text"] = message;
        sendMessage(item.dump());
    } catch (const json::parse_error& e) {
        std::cout << "Failed to parse conversation item JSON: " + std::string(e.what());
    }
}

void CustomWebSocketClient::createResponse()
{
}

void CustomWebSocketClient::cancelAssistantSpeech()
{
    std::string jsonMessage = R"(
    {
        "type": "abort",
        "reason": "wake_word_detected"
    })";

    try {
        json item = json::parse(jsonMessage);
        item["session_id"] = session_id;
        sendMessage(item.dump());
    } catch (const json::parse_error& e) {
        std::cout << "Failed to parse conversation item JSON: " + std::string(e.what());
    }
    clearOutputBuffer();
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
    message += "\"format\":\"opus\", \"sample_rate\":16000, \"channels\":1, \"frame_duration\":" + std::to_string(OPUS_FRAME_DURATION_MS);
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
        auto now = std::chrono::system_clock::now();
        auto t_c = std::chrono::system_clock::to_time_t(now);
        // Parse the JSON message
        json eventJson = json::parse(message);
        eventType = eventJson["type"];

        //std::cout << "[" << std::put_time(std::localtime(&t_c), "%H:%M:%S") << "] receive: " << message << std::endl;
        if (eventType == "hello") {
            session_id = eventJson["session_id"];
            sample_rate = eventJson["audio_params"]["sample_rate"];
            frame_duration = eventJson["audio_params"]["frame_duration"];
            initMediaPlayback();
        } else if (eventType == "tts") {
            if (eventJson["state"] == "sentence_start") {
                std::cout << "[" << std::put_time(std::localtime(&t_c), "%H:%M:%S")  << "] text: " << eventJson["text"] << std::endl;
            } else if (eventJson["state"] == "sentence_end") {
                std::cout << "end of sequence" << std::endl;
                onAudioDone(std::make_shared<ItemType>());
            } 
        }

    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error processing event: " << e.what() << std::endl;
    }
}
