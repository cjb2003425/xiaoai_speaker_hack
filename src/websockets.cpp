#include <libwebsockets.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <nlohmann/json.hpp> // Include nlohmann/json for JSON handling
#include "utils.h"
#include <string.h>
#include <signal.h>

/*
 * This represents your object that "contains" the client connection and has
 * the client connection bound to it
 */

static struct my_conn {
	lws_sorted_usec_list_t	sul;	     /* schedule connection retry */
	struct lws		*wsi;	     /* related wsi if any */
	uint16_t		retry_count; /* count of consequetive retries */
} mco;

static struct lws_context *context;
static int port = 443, ssl_connection = LCCSCF_USE_SSL;

static void connect_client(lws_sorted_usec_list_t *sul);
/*
 * The retry and backoff policy we want to use for our client connections
 */

static const uint32_t backoff_ms[] = { 1000, 2000, 3000, 4000, 5000 };

static const lws_retry_bo_t retry = {
	.retry_ms_table			= backoff_ms,
	.retry_ms_table_count		= LWS_ARRAY_SIZE(backoff_ms),
	.conceal_count			= LWS_ARRAY_SIZE(backoff_ms),

	.secs_since_valid_ping		= 3,  /* force PINGs after secs idle */
	.secs_since_valid_hangup	= 10, /* hangup after secs idle */

	.jitter_percent			= 20,
};

static int callback_websockets(struct lws *wsi, enum lws_callback_reasons reason,
		 void *user, void *in, size_t len)
{
	struct my_conn *mco = (struct my_conn *)user;

	switch (reason) {
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
        lwsl_err("Append handshake headers\n");
        unsigned char **p = (unsigned char **)in, *end = (*p) + len;

        // Example: Add authorization header
        std::string api_key;
        if (get_openai_key(api_key) != 0) {
            lwsl_err("Failed to get OpenAI API Key\n");
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
		lwsl_hexdump_notice(in, len);
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
	if (lws_retry_sul_schedule_retry_wsi(wsi, &mco->sul, connect_client,
					     &mco->retry_count)) {
		lwsl_err("%s: connection attempts exhausted\n", __func__);
	}

	return 0;
}

static const struct lws_protocols protocols[] = {
	{ "wss", callback_websockets, 0, 0, 0, NULL, 0 },
	LWS_PROTOCOL_LIST_TERM
};


/*
 * Scheduled sul callback that starts the connection attempt
 */

static void connect_client(lws_sorted_usec_list_t *sul)
{
	struct my_conn *mco = lws_container_of(sul, struct my_conn, sul);
	struct lws_client_connect_info i;
	std::string url;

    if (get_openai_baseurl(url) != 0) {
        return;
    }
    
	size_t pos = url.find("://");
    std::string hostname = url.substr(pos + 3, url.find('/', pos + 3) - (pos + 3));
    std::string path = url.substr(url.find('/', pos + 3));

    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Path: " << path << std::endl;

	memset(&i, 0, sizeof(i));

	i.context = context;
	i.port = port;
	i.address = hostname.c_str();
	i.path = path.c_str();
	i.host = i.address;
	i.origin = i.address;
	i.ssl_connection = ssl_connection;
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
					   connect_client, &mco->retry_count)) {
			lwsl_err("%s: connection attempts exhausted\n", __func__);
		}
}


int oai_websockets()
{
	struct lws_context_creation_info info;
	const char *p;
	int n = 0;

	memset(&info, 0, sizeof info);
	lwsl_user("LWS minimal ws client\n");

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
	lws_sul_schedule(context, 0, &mco.sul, connect_client, 1);

	while (n >= 0)
		n = lws_service(context, 0);

	lws_context_destroy(context);
	lwsl_user("Completed\n");

	return 0;
}
