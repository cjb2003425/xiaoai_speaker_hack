#include <iostream>
#include <string>
#include <thread>
#include <peer.h>
#include "main.h"
extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

#if 0
static struct ubus_context *ctx = nullptr;

// Event handler callback
void event_handler(struct ubus_context *ctx, struct ubus_event_data *data) {
    printf("Received event: %s\n", data->topic);
}

void ubus_function() {
    struct ubus_event_handler listener;
    ctx = ubus_connect(NULL);
    if (!ctx) {
        std::cerr << "Failed to connect to ubus" << std::endl;
        return;
    }

    // Subscribe to all events
    if (ubus_add_event(ctx, NULL, event_handler) < 0) {
        fprintf(stderr, "Failed to subscribe to events\n");
        ubus_free(ctx);
        return -1;
    }

    // Main loop to process events
    while (1) {
        ubus_dispatch(ctx); // Dispatch events and call handlers
        usleep(100000); // Sleep for 100ms
    }

    std::cout << "exit the loop" << std::endl;
    ubus_free(ctx);
}
#endif


int main(void) {
  peer_init();
  oai_init_audio_capture();
  oai_init_audio_decoder();
  oai_webrtc();
}
