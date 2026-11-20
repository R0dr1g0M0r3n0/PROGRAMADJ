#import "AgentControlInternal.h"

static int exchange(const char *requirement) {
    xpc_connection_t connection = xpc_connection_create(AFServiceIdentifier, nullptr);
    if (!connection) {
        return EXIT_FAILURE;
    }
    if (xpc_connection_set_peer_code_signing_requirement(connection, requirement) != 0) {
        xpc_connection_cancel(connection);
        return EXIT_FAILURE;
    }
    xpc_connection_set_event_handler(connection, ^(xpc_object_t) {});
    xpc_connection_activate(connection);

    dispatch_semaphore_t finished = dispatch_semaphore_create(0);
    __block xpc_object_t reply = nullptr;
    xpc_connection_send_message_with_reply(connection, AFCreateDiscoveryRequest(),
        dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(xpc_object_t value) {
            reply = value;
            dispatch_semaphore_signal(finished);
        });

    constexpr int64_t replyDeadline = 2 * NSEC_PER_SEC;
    bool completed = dispatch_semaphore_wait(finished, dispatch_time(DISPATCH_TIME_NOW, replyDeadline)) == 0;
    xpc_connection_cancel(connection);
    if (!completed) {
        puts("reply timed out");
    } else if (reply == XPC_ERROR_CONNECTION_INTERRUPTED) {
        puts("connection interrupted");
    } else if (reply == XPC_ERROR_PEER_CODE_SIGNING_REQUIREMENT) {
        puts("peer requirement rejected");
    } else if (xpc_get_type(reply) == XPC_TYPE_DICTIONARY) {
        printf("reply: %u\n", static_cast<unsigned>(AFDecodeDiscoveryResponse(reply)));
    } else {
        const char *description = xpc_get_type(reply) == XPC_TYPE_ERROR
            ? xpc_dictionary_get_string(reply, XPC_ERROR_KEY_DESCRIPTION) : nullptr;
        fprintf(stderr, "%s\n", description ? description : "Unexpected XPC object");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc == 2 && strcmp(argv[1], "discover") == 0 && AFVerifiedPeerRequirement(AFEndpointApp)) {
            printf("discovery: %u\n", static_cast<unsigned>(AFDiscoverAgentControl()));
            return EXIT_SUCCESS;
        }
        if (argc == 3 && strcmp(argv[1], "exchange") == 0) {
            return exchange(argv[2]);
        }
        return EXIT_FAILURE;
    }
}
