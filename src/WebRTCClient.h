#ifndef WEBRTC_H
#define WEBRTC_H

#include <string>
#include <atomic>
#include <nlohmann/json.hpp>
#include "peer.h"
#include "RealTimeClient.h"
#include "ThreadTimer.hpp"

// For convenience
using json = nlohmann::json;

class WebRTCClient : public RealTimeClient {
public:
    WebRTCClient();
    ~WebRTCClient();
    
    bool loop() override;
    bool init() override;
    void clearOutputBuffer() override;
    bool deinit();
    bool sendMessage(const std::string& message);
    
private:
    // Constants
    static constexpr int TICK_INTERVAL = 15;
    static constexpr int MAX_EVENT_NAME_LEN = 64;
    static constexpr int MAX_HTTP_OUTPUT_BUFFER = 4096;
    
    // Member variables
    PeerConnection* peer_connection;
    
    // Audio handling
    static void* audioSendTaskWrapper(void* context);
    
    // Callbacks
    static void onOpen(void* userdata);
    static void onClose(void* userdata);
    static void onMessage(char* msg, size_t len, void* userdata, uint16_t sid);
    static void onConnectionStateChange(PeerConnectionState state, void *user_data);
    static void onIceCandidate(char* description, void *user_data);
    
    // Helper functions
    void connectionTimeout();
    void initializePeerConnection();
    void cleanupPeerConnection();
    void onMessage(std::string& message) override;
};

#endif // WEBRTC_H
