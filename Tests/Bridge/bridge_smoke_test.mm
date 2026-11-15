#import <Foundation/Foundation.h>
#import "AudioForgeBridge.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    @autoreleasepool {
        NSString* version = [AudioForgeBridge engineVersion];
        if (![version isEqualToString:@"1.0.0"]) {
            fprintf(stderr, "FAIL: expected engine version 1.0.0, got %s\n",
                    version == nil ? "nil" : version.UTF8String);
            return EXIT_FAILURE;
        }

        if (![AudioForgeBridge supportsEngineAPIMajor:1 minor:0 patch:0] ||
            [AudioForgeBridge supportsEngineAPIMajor:2 minor:0 patch:0]) {
            fprintf(stderr, "FAIL: bridge API version compatibility mapping\n");
            return EXIT_FAILURE;
        }

        NSError *unsupportedError = [AudioForgeBridge
            errorForEngineErrorCode:AudioForgeBridgeErrorCodeUnsupportedProcessing];
        if (![unsupportedError.domain isEqualToString:AudioForgeBridgeErrorDomain] ||
            unsupportedError.code != AudioForgeBridgeErrorCodeUnsupportedProcessing ||
            ![unsupportedError.localizedDescription
                containsString:@"not implemented"]) {
            fprintf(stderr, "FAIL: bridge error mapping\n");
            return EXIT_FAILURE;
        }

        AudioForgeBridgeCancellationToken *token =
            [[AudioForgeBridgeCancellationToken alloc] init];
        if (token.isCancelled) {
            fprintf(stderr, "FAIL: new bridge token starts pending\n");
            return EXIT_FAILURE;
        }
        [token cancel];
        [token cancel];
        if (!token.isCancelled) {
            fprintf(stderr, "FAIL: bridge cancellation token state\n");
            return EXIT_FAILURE;
        }

        AudioForgeBridgeProgress *progress =
            [[AudioForgeBridgeProgress alloc] initWithCompletedUnits:3
                                                           totalUnits:5];
        AudioForgeBridgeProgress *invalidProgress =
            [[AudioForgeBridgeProgress alloc] initWithCompletedUnits:6
                                                           totalUnits:5];
        if (progress.completedUnits != 3 || progress.totalUnits != 5 ||
            invalidProgress != nil) {
            fprintf(stderr, "FAIL: bridge progress snapshot contract\n");
            return EXIT_FAILURE;
        }

        AudioForgeBridgeTimeTransform *transform =
            [[AudioForgeBridgeTimeTransform alloc] initWithSourceStartFrame:10
                                                             sourceFrameCount:100
                                                              outputStartFrame:200
                                                             outputFrameCount:50];
        uint64_t outputFrame = 0;
        if (transform == nil ||
            ![transform mapSourceFrame:35 outputFrame:&outputFrame] ||
            outputFrame != 212 ||
            [transform mapSourceFrame:110 outputFrame:&outputFrame]) {
            fprintf(stderr, "FAIL: bridge time transform mapping\n");
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
