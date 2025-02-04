#ifndef WEBRTC_H
#define WEBRTC_H

#include <string>
#include <atomic>
#include <nlohmann/json.hpp>
#include "main.h"
#include "ThreadTimer.hpp"

// For convenience
using json = nlohmann::json;

class WebRTCManager {
public:
    WebRTCManager(ThreadTimer& timer);
    ~WebRTCManager(){};
    
    void loop();
    bool init();
    int sendMessage(const std::string& message);
    
private:
    // Constants
    static constexpr int TICK_INTERVAL = 15;
    static constexpr int MAX_EVENT_NAME_LEN = 64;
    static constexpr int MAX_HTTP_OUTPUT_BUFFER = 4096;
    
    // Member variables
    PeerConnection* peer_connection;
    json session;
    json conversation;
    std::atomic<bool> request_exit;
    ThreadTimer& timer;
    
    // Audio handling
    static void* audioSendTaskWrapper(void* context);
    
    // Callbacks
    static void onOpen(void* userdata);
    static void onClose(void* userdata);
    static void onMessage(char* msg, size_t len, void* userdata, uint16_t sid);
    static void onMessage(const char* msg, size_t len, uint16_t sid);
    static void onConnectionStateChange(PeerConnectionState state, void *user_data);
    static void onIceCandidate(char* description, void *user_data);
    
    // Helper functions
    void connectionTimeout();
    void initializePeerConnection();
    void cleanupPeerConnection();
};

#endif // WEBRTC_H
