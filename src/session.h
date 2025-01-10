#ifndef SESSION_H
#define SESSION_H

#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp> // Include the nlohmann/json library

// For convenience
using json = nlohmann::json;

// Define the Session class
class Session {
public:
    // Member variables
    std::string id;
    std::string object;
    std::string model;
    long expires_at;
    std::vector<std::string> modalities;
    std::string instructions;
    std::string voice;
    float threshold;
    int prefix_padding_ms;
    int silence_duration_ms;
    bool create_response;
    std::string input_audio_format;
    std::string output_audio_format;
    std::string input_audio_transcription;
    std::string tool_choice;
    float temperature;
    std::string max_response_output_tokens;
    std::string client_secret;
    std::vector<std::string> tools;

    // Parse the JSON string into the class
    void from_json(const json& j);

    // Display the session details
    void print() const;
    void created();
    void update();
};

#endif // SESSION_H

