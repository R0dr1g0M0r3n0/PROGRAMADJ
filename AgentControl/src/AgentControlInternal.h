#pragma once

#import "AudioForgeAgentControl.h"
#import <xpc/xpc.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(uint8_t, AFEndpoint) {
    AFEndpointApp,
    AFEndpointService,
};

FOUNDATION_EXPORT const char *const AFServiceIdentifier;
NSString *_Nullable AFPeerRequirementForTeam(AFEndpoint endpoint, NSString *_Nullable team);
NSString *_Nullable AFVerifiedPeerRequirement(AFEndpoint endpoint);

xpc_object_t AFCreateDiscoveryRequest(void);
AFDiscoveryStatus AFDiscoveryRequestStatus(xpc_object_t message);
void AFEncodeDiscoveryResponse(xpc_object_t response, AFDiscoveryStatus status);
AFDiscoveryStatus AFDecodeDiscoveryResponse(xpc_object_t message);

NS_ASSUME_NONNULL_END
