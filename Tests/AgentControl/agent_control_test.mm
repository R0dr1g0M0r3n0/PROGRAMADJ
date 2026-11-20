#import <Foundation/Foundation.h>
#import "AgentControlInternal.h"

static int failures;

static void check(bool condition, const char *description) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

static void checkEmbeddedService(const char *appPath) {
    NSString *path = [NSString stringWithUTF8String:appPath];
    path = [path stringByAppendingPathComponent:@"Contents/XPCServices/com.audioforge.studio.agent-control.xpc"];
    NSBundle *service = [NSBundle bundleWithPath:path];
    check([service.bundleIdentifier isEqualToString:@"com.audioforge.studio.agent-control"],
          "the app embeds an XPC bundle with the expected identifier");
    NSDictionary *configuration = [service objectForInfoDictionaryKey:@"XPCService"];
    check([configuration[@"ServiceType"] isEqual:@"Application"],
          "launchd scopes the service to the application");
    check(service.executablePath && [[NSFileManager defaultManager] isExecutableFileAtPath:service.executablePath],
          "the embedded service has a runnable executable");
}

static void checkDiscoveryProtocol() {
    xpc_object_t request = AFCreateDiscoveryRequest();
    check(AFDiscoveryRequestStatus(request) == AFDiscoveryStatusSuccess, "accept a v1 discovery request");
    xpc_dictionary_set_uint64(request, "operation", 2);
    check(AFDiscoveryRequestStatus(request) == AFDiscoveryStatusUnsupportedOperation, "reject unimplemented operations");
    xpc_dictionary_set_uint64(request, "version", 2);
    check(AFDiscoveryRequestStatus(request) == AFDiscoveryStatusUnsupportedVersion, "reject unknown protocol versions");
    request = AFCreateDiscoveryRequest();
    xpc_dictionary_set_string(request, "path", "/untrusted");
    check(AFDiscoveryRequestStatus(request) == AFDiscoveryStatusInvalidRequest, "reject unexpected fields");
    request = AFCreateDiscoveryRequest();
    xpc_dictionary_set_string(request, "operation", "1");
    check(AFDiscoveryRequestStatus(request) == AFDiscoveryStatusInvalidRequest, "reject type coercion");
    check(AFDiscoveryRequestStatus(xpc_uint64_create(1)) == AFDiscoveryStatusInvalidRequest, "reject non-dictionaries");

    xpc_object_t response = xpc_dictionary_create_empty();
    AFEncodeDiscoveryResponse(response, AFDiscoveryStatusSuccess);
    check(AFDecodeDiscoveryResponse(response) == AFDiscoveryStatusSuccess, "accept empty capability discovery");
    xpc_array_append_value(xpc_dictionary_get_value(response, "capabilities"), xpc_string_create("play"));
    check(AFDecodeDiscoveryResponse(response) == AFDiscoveryStatusInvalidResponse, "reject unimplemented capability advertisements");
    response = xpc_dictionary_create_empty();
    AFEncodeDiscoveryResponse(response, AFDiscoveryStatusUnsupportedVersion);
    check(AFDecodeDiscoveryResponse(response) == AFDiscoveryStatusUnsupportedVersion, "preserve a typed server error");
    xpc_dictionary_set_uint64(response, "version", 2);
    check(AFDecodeDiscoveryResponse(response) == AFDiscoveryStatusInvalidResponse, "reject responses from an unknown wire version");
    xpc_dictionary_set_uint64(response, "version", 1);
    xpc_dictionary_set_uint64(response, "status", 999);
    check(AFDecodeDiscoveryResponse(response) == AFDiscoveryStatusInvalidResponse, "reject unknown reply statuses");
    check(AFDecodeDiscoveryResponse(XPC_ERROR_CONNECTION_INTERRUPTED) == AFDiscoveryStatusUnavailable, "map interruptions without retry");
    check(AFDecodeDiscoveryResponse(XPC_ERROR_PEER_CODE_SIGNING_REQUIREMENT) == AFDiscoveryStatusUnauthorized, "reject a failed peer-signature check");
}

static void checkIdentityPolicy() {
    NSString *app = AFPeerRequirementForTeam(AFEndpointService, @"TESTTEAM01");
    NSString *service = AFPeerRequirementForTeam(AFEndpointApp, @"TESTTEAM01");
    check([app isEqualToString:@"anchor apple generic and identifier \"com.audioforge.studio\" and certificate leaf[subject.OU] = \"TESTTEAM01\""],
          "the service pins the app ID under the Apple trust anchor and exact team");
    check([service isEqualToString:@"anchor apple generic and identifier \"com.audioforge.studio.agent-control\" and certificate leaf[subject.OU] = \"TESTTEAM01\""],
          "the app pins the helper ID under the Apple trust anchor and exact team");
    check(!AFPeerRequirementForTeam(AFEndpointApp, nil), "reject missing team");
    check(!AFPeerRequirementForTeam(AFEndpointApp, @"testteam01"), "reject noncanonical team identifiers");
    check(!AFPeerRequirementForTeam(AFEndpointApp, @"SHORT"), "reject malformed team length");
    check(!AFPeerRequirementForTeam(AFEndpointApp, @"TESTTEA\"01"), "reject requirement metacharacters");
    check(!AFVerifiedPeerRequirement(AFEndpointService), "do not accept the test executable as the helper");
    check(AFDiscoverAgentControl() == AFDiscoveryStatusUnauthorized, "do not send from an unverified app identity");
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc != 2) {
            return 2;
        }
        checkEmbeddedService(argv[1]);
        checkDiscoveryProtocol();
        checkIdentityPolicy();
        return failures == 0 ? 0 : 1;
    }
}
