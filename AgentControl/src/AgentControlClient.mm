#import "AgentControlInternal.h"

AFDiscoveryStatus AFDiscoverAgentControl(void) {
    NSString *requirement = AFVerifiedPeerRequirement(AFEndpointApp);
    if (!requirement) {
        return AFDiscoveryStatusUnauthorized;
    }

    xpc_connection_t connection = xpc_connection_create(AFServiceIdentifier, nullptr);
    if (!connection) {
        return AFDiscoveryStatusUnavailable;
    }
    if (xpc_connection_set_peer_code_signing_requirement(connection, requirement.UTF8String) != 0) {
        xpc_connection_cancel(connection);
        return AFDiscoveryStatusUnauthorized;
    }

    xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {});
    xpc_connection_activate(connection);

    // The heap-owned block state stays valid even if cancellation delivers a late reply.
    constexpr int64_t replyTimeoutNanoseconds = 2 * NSEC_PER_SEC;
    dispatch_semaphore_t finished = dispatch_semaphore_create(0);
    __block AFDiscoveryStatus result = AFDiscoveryStatusUnavailable;
    xpc_connection_send_message_with_reply(connection, AFCreateDiscoveryRequest(),
        dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(xpc_object_t reply) {
            result = AFDecodeDiscoveryResponse(reply);
            dispatch_semaphore_signal(finished);
        });

    bool completed = dispatch_semaphore_wait(finished, dispatch_time(DISPATCH_TIME_NOW, replyTimeoutNanoseconds)) == 0;
    xpc_connection_cancel(connection);
    return completed ? result : AFDiscoveryStatusTimedOut;
}
