#import "AgentControlInternal.h"
#import <Security/Security.h>

static const char *const appIdentifier = "com.audioforge.studio";
const char *const AFServiceIdentifier = "com.audioforge.studio.agent-control";

static NSString *requirementForIdentifier(const char *identifier, NSString *team) {
    return [NSString stringWithFormat:
        @"anchor apple generic and identifier \"%s\" and certificate leaf[subject.OU] = \"%@\"",
        identifier, team];
}

NSString *AFPeerRequirementForTeam(AFEndpoint endpoint, NSString *team) {
    if (team.length != 10) {
        return nil;
    }
    for (NSUInteger index = 0; index < team.length; ++index) {
        unichar character = [team characterAtIndex:index];
        if (!((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9'))) {
            return nil;
        }
    }
    return requirementForIdentifier(endpoint == AFEndpointApp ? AFServiceIdentifier : appIdentifier, team);
}

static bool checkSelf(SecCodeRef code, NSString *expectedRequirement) {
    SecRequirementRef requirement = nullptr;
    if (SecRequirementCreateWithString((__bridge CFStringRef)expectedRequirement,
                                       kSecCSDefaultFlags, &requirement) != errSecSuccess) {
        return false;
    }
    bool valid = SecCodeCheckValidity(code, kSecCSNoNetworkAccess, requirement) == errSecSuccess;
    CFRelease(requirement);
    return valid;
}

NSString *AFVerifiedPeerRequirement(AFEndpoint endpoint) {
    const char *identifier = endpoint == AFEndpointApp ? appIdentifier : AFServiceIdentifier;
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@(identifier)]) {
        return nil;
    }

    SecCodeRef selfCode = nullptr;
    if (SecCodeCopySelf(kSecCSDefaultFlags, &selfCode) != errSecSuccess) {
        return nil;
    }
    CFDictionaryRef information = nullptr;
    if (SecCodeCopySigningInformation(selfCode, kSecCSSigningInformation, &information) != errSecSuccess) {
        CFRelease(selfCode);
        return nil;
    }

    NSDictionary *signing = CFBridgingRelease(information);
    id team = signing[(__bridge NSString *)kSecCodeInfoTeamIdentifier];
    NSString *peerRequirement = [team isKindOfClass:NSString.class] ? AFPeerRequirementForTeam(endpoint, team) : nil;
    bool valid = peerRequirement && checkSelf(selfCode, requirementForIdentifier(identifier, team));
    CFRelease(selfCode);
    return valid ? peerRequirement : nil;
}
