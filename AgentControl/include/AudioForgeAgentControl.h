#pragma once

#import <Foundation/Foundation.h>

typedef NS_ENUM(int32_t, AFDiscoveryStatus) {
    AFDiscoveryStatusSuccess = 0,
    AFDiscoveryStatusInvalidRequest = 1,
    AFDiscoveryStatusUnsupportedVersion = 2,
    AFDiscoveryStatusUnsupportedOperation = 3,
    AFDiscoveryStatusUnauthorized = 4,
    AFDiscoveryStatusUnavailable = 5,
    AFDiscoveryStatusTimedOut = 6,
    AFDiscoveryStatusInvalidResponse = 7,
};

// One discovery request, no retries. Call off the UI/audio threads.
// Success means authenticated protocol v1, with no product capabilities yet.
FOUNDATION_EXPORT AFDiscoveryStatus AFDiscoverAgentControl(void);
