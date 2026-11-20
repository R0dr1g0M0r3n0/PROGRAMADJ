#import "AgentControlInternal.h"

static NSString *appRequirement;

static void acceptPeer(xpc_connection_t peer) {
    if (xpc_connection_set_peer_code_signing_requirement(peer, appRequirement.UTF8String) != 0) {
        xpc_connection_cancel(peer);
        return;
    }
    __block bool responded = false;
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
        if (xpc_get_type(event) != XPC_TYPE_DICTIONARY) {
            return;
        }
        xpc_connection_t sender = xpc_dictionary_get_remote_connection(event);
        if (!sender) {
            return;
        }
        if (responded) {
            xpc_connection_cancel(sender);
            return;
        }
        responded = true;
        xpc_object_t reply = xpc_dictionary_create_reply(event);
        if (!reply) {
            xpc_connection_cancel(sender);
            return;
        }

        AFEncodeDiscoveryResponse(reply, AFDiscoveryRequestStatus(event));
        xpc_connection_send_message(sender, reply);
    });
    xpc_connection_resume(peer);
}

int main() {
    @autoreleasepool {
        appRequirement = AFVerifiedPeerRequirement(AFEndpointService);
        if (!appRequirement) {
            return EXIT_FAILURE;
        }
        xpc_main(acceptPeer);
    }
}
