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
    }
    return EXIT_SUCCESS;
}
