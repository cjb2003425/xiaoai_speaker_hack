#include <libwebsockets.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <string.h>
#include <sstream>
#include <fstream>
#include <chrono>
#include "Utils.h"
#include "WebSocketClient.h"
#include "main.h"

using json = nlohmann::json;


struct msg {
    void *payload; /* is malloc'd */
    size_t len;
};

const uint32_t WebSocketClient::backoff_ms[BACKOFF_MS_NUM] = { 1000, 2000, 3000, 4000, 5000 };

const lws_retry_bo_t WebSocketClient::retry = {
    .retry_ms_table            = backoff_ms,
    .retry_ms_table_count        = LWS_ARRAY_SIZE(backoff_ms),
    .conceal_count            = 0xffff,

    .secs_since_valid_ping        = 3,  /* force PINGs after secs idle */
    .secs_since_valid_hangup    = 10, /* hangup after secs idle */

    .jitter_percent            = 20,
};

void WebSocketClient::destroy_message(void *_msg)
{
    struct msg *msg = static_cast<struct msg *>(_msg);

    free(msg->payload);
    msg->payload = NULL;
    msg->len = 0;
}

int WebSocketClient::callback(struct lws *wsi, enum lws_callback_reasons reason,
         void *user, void *in, size_t len)
{
    ConnectionInfo* con = static_cast<ConnectionInfo*>(user);
    lws_context *context = lws_get_context(wsi);
    WebSocketClient* client = static_cast<WebSocketClient*>(lws_context_user(context));
    const struct msg *pmsg;
    int m = 0;

    switch (reason) {
    case LWS_CALLBACK_PROTOCOL_DESTROY:
        lwsl_info("protocol destroy\n");
        if (con->ring)
            lws_ring_destroy(con->ring);

        pthread_mutex_destroy(&con->lock_ring);

        return 0;
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
        lwsl_info("Append handshake headers\n");
        unsigned char **p = (unsigned char **)in, *end = (*p) + len;

        // Example: Add authorization header
        std::string api_key;
        if (!Utils::get_openai_key(api_key)) {
            return -1;
        }
        std::string auth_header = "Bearer " + api_key;
        if (lws_add_http_header_by_name(wsi,
                (const unsigned char *)"Authorization:",
                (const unsigned char *)auth_header.c_str(),
                auth_header.length(), p, end)) {
            lwsl_err("Failed to append authorization header\n");
            return -1;
        }
        
        // Example: Add custom header
        const char *custom_header = "realtime=v1";
        if (lws_add_http_header_by_name(wsi,
                (const unsigned char *)"OpenAI-Beta:",
                (const unsigned char *)custom_header,
                strlen(custom_header), p, end)) {
            lwsl_err("Failed to append custom header\n");
            return -1;
        }
        break;
    }
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        lwsl_err("connection error: %s\n",
             in ? (char *)in : "(null)");
        goto do_retry;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        lwsl_info("client receive\n");
        if (client) {
            // Check if the received data is binary
            bool is_binary = lws_frame_is_binary(wsi);
            lwsl_info("Received %s data\n", is_binary ? "binary" : "text");

            // Handle binary data differently from text data
            if (is_binary) {
                // Handle binary data directly
                client->onBinaryMessage((const uint8_t*)in, len);
            } else {
                // Handle text data (existing JSON processing)
                client->recvmsg += std::string((char*)in, len);

                // Check if we have a complete JSON message
                int openBraces = 0;
                int closeBraces = 0;
                for (char c : client->recvmsg) {
                    if (c == '{') {
                        openBraces++;
                    } else if (c == '}') {
                        closeBraces++;
                    }
                }

                // If the number of opening and closing braces match, we have a complete JSON message
                if (openBraces > 0 && openBraces == closeBraces) {
                    client->onMessage(client->recvmsg); // Process the complete message
                    client->recvmsg.clear(); // Clear the buffer for the next message
                } else {
                    lwsl_info("Received: %s \n", in);
                }
            }
        }
        break;
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        con->established = 1;
        lwsl_user("%s: established\n", __func__);
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        lwsl_info("client writeable\n");
        pthread_mutex_lock(&con->lock_ring); /* --------- ring lock { */
        pmsg = static_cast<const msg*>(lws_ring_get_element(con->ring, &con->tail));
        if (!pmsg) {
            lwsl_info("no data to send\n");
            goto skip;
        }

        /* notice we allowed for LWS_PRE in the payload already */
        m = lws_write(wsi, ((unsigned char *)pmsg->payload) + LWS_PRE,
                  pmsg->len, LWS_WRITE_TEXT);
        if (m < (int)pmsg->len) {
            pthread_mutex_unlock(&con->lock_ring); /* } ring lock */
            lwsl_err("ERROR %d writing to ws socket\n", m);
            return -1;
        }

        lws_ring_consume_single_tail(con->ring, &con->tail, 1);

        /* more to do for us? */
        if (lws_ring_get_element(con->ring, &con->tail))
            /* come back as soon as we can write more */
            lws_callback_on_writable(wsi);

skip:
        pthread_mutex_unlock(&con->lock_ring); /* } ring lock ------- */
        break;


    case LWS_CALLBACK_CLIENT_CLOSED:
        lwsl_info("client closed\n");
        con->established = 0;
        client->conversation.clear();
        goto do_retry;

    case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
        lwsl_info("peer initiated close\n");
        break;

    case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
        lwsl_info("event wait canclelled\n");
        if (con && con->wsi && con->established)
            lws_callback_on_writable(con->wsi);
        break;

    default:
        break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);

do_retry:
    /*
     * retry the connection to keep it nailed up
     *
     * For this example, we try to conceal any problem for one set of
     * backoff retries and then exit the app.
     *
     * If you set retry.conceal_count to be larger than the number of
     * elements in the backoff table, it will never give up and keep
     * retrying at the last backoff delay plus the random jitter amount.
     */
    if (lws_retry_sul_schedule_retry_wsi(wsi, &con->sul, connectClient,
                         &con->retry_count)) {
        lwsl_err("%s: connection attempts exhausted\n", __func__);
    }

    return 0;
}

const struct lws_protocols WebSocketClient::protocols[2] = {
    { "lws-client", callback, 0, 0, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM
};


/*
 * Scheduled sul callback that starts the connection attempt
 */

void WebSocketClient::connectClient(lws_sorted_usec_list_t *sul)
{
    struct ConnectionInfo *mco = lws_container_of(sul, struct ConnectionInfo, sul);
    struct lws_client_connect_info i;
    std::string url;

    if (!Utils::get_openai_baseurl(url)) {
        return;
    }
    
    std::string hostname;
    std::string path;
    std::string protocol;

    // Find protocol separator
    size_t pos = url.find("://");
    if (pos == std::string::npos) {
        lwsl_err("Invalid URL format: missing protocol separator\n");
        return;
    }

    // Extract protocol
    protocol = url.substr(0, pos);
    if (protocol != "wss" && protocol != "ws") {
        lwsl_err("Invalid WebSocket protocol: %s (must be 'ws' or 'wss')\n", protocol.c_str());
        return;
    }

    // Log the connection details
    std::cout << "Protocol: " << protocol << std::endl;
    std::cout << "Using SSL: " << (protocol == "wss" ? "yes" : "no") << std::endl;

    // Find the first '/' after the host
    size_t pathPos = url.find('/', pos + 3);
    if (pathPos == std::string::npos) {
        // No path specified, use root path
        hostname = url.substr(pos + 3);
        path = "/";
    } else {
        hostname = url.substr(pos + 3, pathPos - (pos + 3));
        path = url.substr(pathPos);
    }

    // Remove any trailing port number from hostname if present
    size_t portPos = hostname.find(':');
    if (portPos != std::string::npos) {
        hostname = hostname.substr(0, portPos);
    }

    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Path: " << path << std::endl;
    std::cout << "Port: " << mco->port << std::endl;

    memset(&i, 0, sizeof(i));

    i.context = mco->context;
    i.port = mco->port;
    i.address = hostname.c_str();
    i.path = path.c_str();
    i.host = i.address;
    i.origin = i.address;
    i.ssl_connection = (protocol == "wss") ? LCCSCF_USE_SSL : 0;
    i.protocol = NULL;
    i.pwsi = &mco->wsi;
    i.retry_and_idle_policy = &retry;
    i.userdata = mco;

    if (!lws_client_connect_via_info(&i))
        /*
         * Failed... schedule a retry... we can't use the _retry_wsi()
         * convenience wrapper api here because no valid wsi at this
         * point.
         */
        if (lws_retry_sul_schedule(mco->context, 0, sul, &retry,
                       connectClient, &mco->retry_count)) {
            lwsl_err("%s: connection attempts exhausted\n", __func__);
        }
}


bool WebSocketClient::init()
{
    struct lws_context_creation_info info;
    const char *p;
    int n = 0;
    int log_level = LLL_ERR | LLL_WARN | LLL_NOTICE; 

    // Set the log level and optional log callback
    lws_set_log_level(log_level, NULL);

    memset(&info, 0, sizeof info);
    std::cout << "WebSocketClient connect!" << std::endl;

    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
    info.protocols = protocols;
    info.fd_limit_per_thread = 1 + 1 + 1;
    info.user = this;

    mConnectionInfo.context = lws_create_context(&info);
    if (!mConnectionInfo.context) {
        lwsl_err("lws init failed\n");
        return true;
    }

    /* schedule the first client connection attempt to happen immediately */
    lws_sul_schedule(mConnectionInfo.context, 0, &mConnectionInfo.sul, connectClient, 1);
    return true;
}

bool WebSocketClient::loop() {
    int n = 0;
    while (n >= 0 && !quitRequest) {
        n = lws_service(mConnectionInfo.context, 0);
    }
    return true;
}

WebSocketClient::WebSocketClient() {
    memset(&mConnectionInfo, 0, sizeof(mConnectionInfo));
    mConnectionInfo.ring = lws_ring_create(sizeof(struct msg), MAX_RINGBUFFER_SIZE,
                    destroy_message);
    pthread_mutex_init(&mConnectionInfo.lock_ring, NULL);
    
    // Start audio processing thread
    audioThreadRunning = true;
    responseDone = false;
    audioBufferSize = 0;
    setListeningPort(PORT);
    audioThread = std::thread(&WebSocketClient::audioProcessingThread, this);
}

WebSocketClient::~WebSocketClient() {
    stopAudioThread();
    
    if (mConnectionInfo.ring) {
        lws_ring_destroy(mConnectionInfo.ring);
    }

    if (mConnectionInfo.context) {
        lws_context_destroy(mConnectionInfo.context);
    }
    pthread_mutex_destroy(&mConnectionInfo.lock_ring);
}

bool WebSocketClient::sendMessage(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto t_c = std::chrono::system_clock::to_time_t(now);
    
    std::cout << "[" << std::put_time(std::localtime(&t_c), "%Y-%m-%d %H:%M:%S") 
              << "] send: " << message << std::endl;
    
    size_t len = message.length() + 1;
    struct msg amsg;
    int index = 1;
    if (!mConnectionInfo.established) {
        return false;
    }

    pthread_mutex_lock(&mConnectionInfo.lock_ring); /* --------- ring lock { */

    /* only create if space in ringbuffer */
    int n = (int)lws_ring_get_count_free_elements(mConnectionInfo.ring);
    if (!n) {
        return false;
    }

    amsg.payload = malloc(LWS_PRE + len);
    if (!amsg.payload) {
        lwsl_user("OOM: dropping\n");
        return false;
    }
    n = lws_snprintf((char *)amsg.payload + LWS_PRE, len,
                 "%s",
                 message.c_str());
    amsg.len = n;
    n = lws_ring_insert(mConnectionInfo.ring, &amsg, 1);
    if (n != 1) {
        destroy_message(&amsg);
        lwsl_user("dropping!\n");
    } else
        /*
         * This will cause a LWS_CALLBACK_EVENT_WAIT_CANCELLED
         * in the lws service thread context.
         */
        lws_cancel_service(mConnectionInfo.context);

    pthread_mutex_unlock(&mConnectionInfo.lock_ring); /* } ring lock ------- */
    return true;
}

void WebSocketClient::onMessage(std::string& message) {
    RealTimeClient::onMessage(message);
}

void WebSocketClient::onBinaryMessage(const uint8_t* data, size_t len) {
    if (!wakeupOn) {
        auto deltaPart = std::make_shared<ItemContentDeltaType>();
        deltaPart->audio.append(data, len);
        onAudioDelta(deltaPart);
    }
}

void WebSocketClient::onAudioDelta(std::shared_ptr<ItemContentDeltaType> delta) {
    #ifdef AUDIO_DEBUG
    static int fileIndex = 0;

    // Get the current time
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    // Create a unique filename using the current time and file index
    std::ostringstream oss;
    oss << "audio_delta_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_" << fileIndex++ << ".txt";
    std::string filename = oss.str();

    // Write the content to the file
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        outFile << delta->audio;
        outFile.close();
    } else {
        std::cerr << "Failed to open file: " << filename << std::endl;
    }
    #else
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        deltaQueue.push(delta);
        audioBufferSize += delta->audio.size();
    }
    audioCondVar.notify_one();
    responseDone = false;
    #endif
}

void WebSocketClient::onAudioDone(std::shared_ptr<ItemType> item) {
    responseDone = true;
}

void WebSocketClient::audioProcessingThread() {
    while (audioThreadRunning) {
        std::shared_ptr<ItemContentDeltaType> delta;
        AudioBuffer* buffer = nullptr;
        {
            std::unique_lock<std::mutex> lock(audioMutex);
            audioCondVar.wait(lock, [this] { 
                if (!audioThreadRunning) {
                    return true;
                }

                if (!responseDone) {
                    if (audioBufferSize > MAX_BUFF_DELTA) {
                        return true;
                    }
                } else {
                    if (!deltaQueue.empty()) {
                        return true;
                    }
                }
                return false;
            });
            
            if (!audioThreadRunning) {
                break;
            }
            
            delta = std::move(deltaQueue.front());
            if (delta) {
                buffer = &delta->audio;
            }
            deltaQueue.pop();
            audioBufferSize -= delta->audio.size();
        }
        
        if (buffer->size() > 0 && !wakeupOn) {
            if (!rawAudio) {
                // Decode the audio data
                oai_audio_decode(buffer->get(), buffer->size());
            } else {
                oai_audio_write(buffer->get(), buffer->size() / 2);
                // Write the raw audio data directly
            }
        }
    }
}

void WebSocketClient::stopAudioThread() {
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        audioThreadRunning = false;
    }
    audioCondVar.notify_one();
    
    if (audioThread.joinable()) {
        audioThread.join();
    }
}

void WebSocketClient::clearOutputBuffer() {
    std::unique_lock<std::mutex> lock(audioMutex);
    while (!deltaQueue.empty()) {
        deltaQueue.pop();
    }
    audioBufferSize = 0;
}

void WebSocketClient::onClientEstablished()
{
}

void WebSocketClient::onClientClosed()
{
}

bool WebSocketClient::quit() {
    quitRequest = true;
    stopAudioThread();
    return true;
}

void WebSocketClient::setListeningPort(int port) {
    mConnectionInfo.port = port;
}