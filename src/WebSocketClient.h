#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <libwebsockets.h>
#include <string>
#include <nlohmann/json.hpp>

class WebSocketClient {
public:
    // Constructor and Destructor
    WebSocketClient(){};
    ~WebSocketClient(){};

    // Public methods
    int  connect(); // Initiate a connection

private:
    static constexpr int PORT = 443;
    static constexpr int SSL_CONNECTION = LCCSCF_USE_SSL;
    static constexpr int BACKOFF_MS_NUM = 5;
    // Private methods
    static int callback(struct lws *wsi, enum lws_callback_reasons reason,
                        void *user, void *in, size_t len);
    static void onMessage(const char* message, size_t len);

    static void connectClient(lws_sorted_usec_list_t *sul);
    void updateSession();

    // Data structures
    struct ConnectionInfo {
        lws_sorted_usec_list_t sul;    // Schedule connection retry
        struct lws *wsi;               // Related wsi if any
        uint16_t retry_count;          // Count of consecutive retries
    };

    // Private members
    static nlohmann::json session;
    static ConnectionInfo mConnectionInfo;
    static struct lws_context *context;
    static const struct lws_protocols protocols[2]; 
    static const uint32_t backoff_ms[BACKOFF_MS_NUM];
    static const lws_retry_bo_t retry; // Declare the retry as a static class member
};

#endif // WEBSOCKETCLIENT_H