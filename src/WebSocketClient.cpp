#include <libwebsockets.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <nlohmann/json.hpp> // Include nlohmann/json for JSON handling
#include <string.h>
#include "utils.h"
#include "WebSocketClient.h"

using json = nlohmann::json;

const uint32_t WebSocketClient::backoff_ms[BACKOFF_MS_NUM] = { 1000, 2000, 3000, 4000, 5000 };
nlohmann::json WebSocketClient::session;
WebSocketClient::ConnectionInfo WebSocketClient::mConnectionInfo;
struct lws_context * WebSocketClient::context = nullptr;

const lws_retry_bo_t WebSocketClient::retry = {
	.retry_ms_table			= backoff_ms,
	.retry_ms_table_count		= LWS_ARRAY_SIZE(backoff_ms),
	.conceal_count			= LWS_ARRAY_SIZE(backoff_ms),

	.secs_since_valid_ping		= 3,  /* force PINGs after secs idle */
	.secs_since_valid_hangup	= 10, /* hangup after secs idle */

	.jitter_percent			= 20,
};

void WebSocketClient::updateSession() {
	session["turn_detection"] = nullptr;
}

void WebSocketClient::onMessage(const char* message, size_t len) {
	std::string json_data = std::string(message, len);
    try {
        // Parse the JSON message
        json event = json::parse(json_data);
      	std::cout << "message type is " << event["type"] << std::endl;
        if (event["type"] == "session.created") {
            session = event["session"];
            std::cout << json_data << std::endl;
        } else if (event["type"] == "session.updated") {
            session = event["session"];
        } else if (event["type"] == "conversation.item.created") {
        } else if (event["type"] == "response.audio_transcript.delta") {
            //std::cout << event["delta"] << std::endl;
        } else if (event["type"] == "response.audio_transcript.done") {
            std::cout << event["transcript"] << std::endl;
        } else if (event["type"] == "response.done") {
            std::cout << event["response"]["usage"] << std::endl;
        } else if (event["type"] == "error") {
            std::cout << json_data << std::endl;
        } else {
            //std::cout << json_data << std::endl;
        }
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }

}

int WebSocketClient::callback(struct lws *wsi, enum lws_callback_reasons reason,
		 void *user, void *in, size_t len)
{
	ConnectionInfo* con = static_cast<ConnectionInfo*>(user);

	switch (reason) {
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
        lwsl_err("Append handshake headers\n");
        unsigned char **p = (unsigned char **)in, *end = (*p) + len;

        // Example: Add authorization header
        std::string api_key;
        if (!get_openai_key(api_key)) {
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
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		goto do_retry;
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		lwsl_err("CLIENT_CONNECTION_RECEIVE\n");
		onMessage((const char*)in, len);
		break;
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_user("%s: established\n", __func__);
		break;

	case LWS_CALLBACK_CLIENT_CLOSED:
		goto do_retry;

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
	{ "wss", callback, 0, 0, 0, NULL, 0 },
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

    if (!get_openai_baseurl(url)) {
        return;
    }
    
	size_t pos = url.find("://");
    std::string hostname = url.substr(pos + 3, url.find('/', pos + 3) - (pos + 3));
    std::string path = url.substr(url.find('/', pos + 3));

    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Path: " << path << std::endl;

	memset(&i, 0, sizeof(i));

	i.context = context;
	i.port = PORT;
	i.address = hostname.c_str();
	i.path = path.c_str();
	i.host = i.address;
	i.origin = i.address;
	i.ssl_connection = SSL_CONNECTION;
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
		if (lws_retry_sul_schedule(context, 0, sul, &retry,
					   connectClient, &mco->retry_count)) {
			lwsl_err("%s: connection attempts exhausted\n", __func__);
		}
}


int WebSocketClient::connect()
{
	struct lws_context_creation_info info;
	const char *p;
	int n = 0;

	memset(&info, 0, sizeof info);
	std::cout << "WebSocketClient connect!" << std::endl;

	info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
	info.fd_limit_per_thread = 1 + 1 + 1;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	/* schedule the first client connection attempt to happen immediately */
	lws_sul_schedule(context, 0, &mConnectionInfo.sul, connectClient, 1);

	while (n >= 0)
		n = lws_service(context, 0);

	lws_context_destroy(context);
	lwsl_user("Completed\n");

	return 0;
}
