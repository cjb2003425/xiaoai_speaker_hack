#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <libwebsockets.h>
#include <string>
#include "RealTimeClient.h"
#include <nlohmann/json.hpp>

class WebSocketClient : public RealTimeClient {
public:
    // Constructor and Destructor
    WebSocketClient();
    ~WebSocketClient();

    // Public methods
    bool init(); // Initiate a connection
    bool loop(); 
    bool sendMessage(const std::string& message);

private:
    static constexpr int PORT = 443;
    static constexpr int SSL_CONNECTION = LCCSCF_USE_SSL;
    static constexpr int BACKOFF_MS_NUM = 5;
    // Private methods
    static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                        void *user, void *in, size_t len);

    static void connectClient(lws_sorted_usec_list_t *sul);
    static void destroy_message(void *_msg);
    void updateSession();
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
    static const struct lws_protocols protocols[2]; 
    static const uint32_t backoff_ms[BACKOFF_MS_NUM];
    static const lws_retry_bo_t retry; // Declare the retry as a static class member
};

#endif // WEBSOCKETCLIENT_H