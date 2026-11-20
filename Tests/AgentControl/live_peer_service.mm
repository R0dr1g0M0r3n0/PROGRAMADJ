#import "AgentControlInternal.h"

// This app-scoped test fixture only answers the empty v1 discovery request.
static void acceptTestPeer(xpc_connection_t peer) {
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
        if (xpc_get_type(event) != XPC_TYPE_DICTIONARY) {
            return;
        }
        xpc_object_t reply = xpc_dictionary_create_reply(event);
        if (reply) {
            AFEncodeDiscoveryResponse(reply, AFDiscoveryRequestStatus(event));
            xpc_connection_send_message(xpc_dictionary_get_remote_connection(event), reply);
        }
    });
    xpc_connection_resume(peer);
}

int main() {
    xpc_main(acceptTestPeer);
}
