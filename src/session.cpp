#include "session.h"

Session::Session(PeerConnection* pc)
{
    peer_connection = pc;
}

// Parse the JSON string into the class
void Session::from_json(const json& j) {
    id = j["id"];
    object = j["object"];
    model = j["model"];
    expires_at = j["expires_at"];
    modalities = j["modalities"].get<std::vector<std::string>>();
    instructions = j["instructions"];
    voice = j["voice"];
    threshold = j["turn_detection"]["threshold"];
    prefix_padding_ms = j["turn_detection"]["prefix_padding_ms"];
    silence_duration_ms = j["turn_detection"]["silence_duration_ms"];
    create_response = j["turn_detection"]["create_response"];
    input_audio_format = j["input_audio_format"];
    output_audio_format = j["output_audio_format"];
    input_audio_transcription = j["input_audio_transcription"].is_null() ? "" : j["input_audio_transcription"];
    tool_choice = j["tool_choice"];
    temperature = j["temperature"];
    max_response_output_tokens = j["max_response_output_tokens"];
    client_secret = j["client_secret"].is_null() ? "" : j["client_secret"];
    tools = j["tools"].get<std::vector<std::string>>();
}

// Display the session details
void Session::print() const {
    std::cout << "Session ID: " << id << "\n"
              << "Object: " << object << "\n"
              << "Model: " << model << "\n"
              << "Expires At: " << expires_at << "\n"
              << "Modalities: ";
    for (const auto& modality : modalities) {
        std::cout << modality << " ";
    }
    std::cout << "\nInstructions: " << instructions << "\n"
              << "Voice: " << voice << "\n"
              << "Threshold: " << threshold << "\n"
              << "Prefix Padding (ms): " << prefix_padding_ms << "\n"
              << "Silence Duration (ms): " << silence_duration_ms << "\n"
              << "Create Response: " << (create_response ? "true" : "false") << "\n"
              << "Input Audio Format: " << input_audio_format << "\n"
              << "Output Audio Format: " << output_audio_format << "\n"
              << "Input Audio Transcription: " << input_audio_transcription << "\n"
              << "Tool Choice: " << tool_choice << "\n"
              << "Temperature: " << temperature << "\n"
              << "Max Response Output Tokens: " << max_response_output_tokens << "\n"
              << "Client Secret: " << client_secret << "\n"
              << "Tools: ";
    for (const auto& tool : tools) {
        std::cout << tool << " ";
    }
    std::cout << std::endl;
}

void Session::created() {

}

void Session::update() {
    // Create a client event
    json responseCreate = {
        {"type", "response.create"},
        {"response", {
            {"modalities", {"text"}},
            {"instructions", "Write a haiku about code"}
        }}
    };
    // Serialize to JSON and send
    std::string message = responseCreate.dump();
    peer_connection_datachannel_send(peer_connection, const_cast<char*>(message.c_str()), message.size());
}

