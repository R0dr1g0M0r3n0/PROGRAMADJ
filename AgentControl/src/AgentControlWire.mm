#import "AgentControlInternal.h"

static constexpr uint64_t protocolVersion = 1;
static constexpr uint64_t discoverOperation = 1;

static bool hasUnsignedField(xpc_object_t dictionary, const char *key) {
    xpc_object_t value = xpc_dictionary_get_value(dictionary, key);
    return value && xpc_get_type(value) == XPC_TYPE_UINT64;
}

xpc_object_t AFCreateDiscoveryRequest(void) {
    xpc_object_t request = xpc_dictionary_create_empty();
    xpc_dictionary_set_uint64(request, "version", protocolVersion);
    xpc_dictionary_set_uint64(request, "operation", discoverOperation);
    return request;
}

AFDiscoveryStatus AFDiscoveryRequestStatus(xpc_object_t message) {
    if (xpc_get_type(message) != XPC_TYPE_DICTIONARY ||
        xpc_dictionary_get_count(message) != 2 ||
        !hasUnsignedField(message, "version") ||
        !hasUnsignedField(message, "operation")) {
        return AFDiscoveryStatusInvalidRequest;
    }
    if (xpc_dictionary_get_uint64(message, "version") != protocolVersion) {
        return AFDiscoveryStatusUnsupportedVersion;
    }
    return xpc_dictionary_get_uint64(message, "operation") == discoverOperation
        ? AFDiscoveryStatusSuccess : AFDiscoveryStatusUnsupportedOperation;
}

void AFEncodeDiscoveryResponse(xpc_object_t response, AFDiscoveryStatus status) {
    xpc_dictionary_set_uint64(response, "version", protocolVersion);
    xpc_dictionary_set_uint64(response, "status", status);
    if (status == AFDiscoveryStatusSuccess) {
        xpc_dictionary_set_value(response, "capabilities", xpc_array_create_empty());
    }
}

AFDiscoveryStatus AFDecodeDiscoveryResponse(xpc_object_t message) {
    if (message == XPC_ERROR_PEER_CODE_SIGNING_REQUIREMENT) {
        return AFDiscoveryStatusUnauthorized;
    }
    if (xpc_get_type(message) == XPC_TYPE_ERROR) {
        return AFDiscoveryStatusUnavailable;
    }
    if (xpc_get_type(message) != XPC_TYPE_DICTIONARY ||
        !hasUnsignedField(message, "version") ||
        xpc_dictionary_get_uint64(message, "version") != protocolVersion ||
        !hasUnsignedField(message, "status")) {
        return AFDiscoveryStatusInvalidResponse;
    }

    uint64_t status = xpc_dictionary_get_uint64(message, "status");
    switch (status) {
        case AFDiscoveryStatusSuccess: {
            xpc_object_t capabilities = xpc_dictionary_get_value(message, "capabilities");
            return xpc_dictionary_get_count(message) == 3 && capabilities &&
                xpc_get_type(capabilities) == XPC_TYPE_ARRAY && xpc_array_get_count(capabilities) == 0
                ? AFDiscoveryStatusSuccess : AFDiscoveryStatusInvalidResponse;
        }
        case AFDiscoveryStatusInvalidRequest:
        case AFDiscoveryStatusUnsupportedVersion:
        case AFDiscoveryStatusUnsupportedOperation:
            return xpc_dictionary_get_count(message) == 2
                ? static_cast<AFDiscoveryStatus>(status)
                : AFDiscoveryStatusInvalidResponse;
        default:
            return AFDiscoveryStatusInvalidResponse;
    }
}
