#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <queue>
#include <libwebsockets.h>
#include <string>
#include "RealTimeClient.h"
#include <nlohmann/json.hpp>
#include <mutex>
#include <condition_variable>
#include <thread>

class WebSocketClient : public RealTimeClient {
public:
    static constexpr int MAX_RINGBUFFER_SIZE = 8;
    // Constructor and Destructor
    WebSocketClient();
    ~WebSocketClient();

    // Public methods
    bool init() override; // Initiate a connection
    bool loop() override; // Loop to process events
    bool sendMessage(const std::string& message) override;
    void onAudioDelta(std::shared_ptr<ItemContentDeltaType> delta) override;
    void onAudioDone(std::shared_ptr<ItemType> item) override;

private:
    static constexpr int PORT = 443;
    static constexpr int SSL_CONNECTION = LCCSCF_USE_SSL;
    static constexpr int BACKOFF_MS_NUM = 5;
    static constexpr int MAX_BUFF_DELTA = 2;
    // Private methods
    static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                        void *user, void *in, size_t len);

    static void connectClient(lws_sorted_usec_list_t *sul);
    static void destroy_message(void *_msg);
    void onMessage(std::string& message) override;

    // Data structures
    struct ConnectionInfo {
        lws_sorted_usec_list_t sul;    // Schedule connection retry
        uint16_t retry_count;          // Count of consecutive retries
        struct lws *wsi;               // Related wsi if any
        struct lws_context *context;
        struct lws_ring * ring;
        pthread_mutex_t lock_ring; /* serialize access to the ring buffer */
        uint32_t tail;
        bool established;
    };

    // Private members
    ConnectionInfo mConnectionInfo;
    std::string recvmsg;
    bool startFlag = false;
    bool endFlag = false;
    static const struct lws_protocols protocols[2]; 
    static const uint32_t backoff_ms[BACKOFF_MS_NUM];
    static const lws_retry_bo_t retry; // Declare the retry as a static class member

    // Audio queue members
    std::queue<std::shared_ptr<ItemContentDeltaType>> deltaQueue;
    std::mutex audioMutex;
    std::condition_variable audioCondVar;
    std::thread audioThread;
    bool audioThreadRunning;
    bool responseDone;

    void audioProcessingThread();
    void stopAudioThread();
};

#endif // WEBSOCKETCLIENT_H