#import "AudioForgeBridge.h"

#include "audioforge/engine.h"

@implementation AudioForgeBridge

+ (NSString *)engineVersion {
    AudioForgeVersion version = audioforge_engine_get_version();
    return [NSString stringWithFormat:@"%u.%u.%u", version.major, version.minor,
                                      version.patch];
}

@end
