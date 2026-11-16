#import "AudioForgeBridge.h"

#include "audioforge/engine.h"

NSErrorDomain const AudioForgeBridgeErrorDomain =
    @"com.audioforge.bridge.error";

@implementation AudioForgeBridgeCancellationToken {
    AudioForgeCancellationToken _token;
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _token = AudioForgeCancellationToken AUDIOFORGE_CANCELLATION_TOKEN_INIT;
    }
    return self;
}

- (void)cancel {
    (void)audioforge_cancellation_token_cancel(&_token);
}

- (BOOL)isCancelled {
    int32_t cancelled = 0;
    if (audioforge_cancellation_token_is_cancelled(&_token, &cancelled) !=
        AUDIOFORGE_STATUS_OK) {
        return NO;
    }
    return cancelled != 0;
}

@end

@implementation AudioForgeBridgeProgress

- (nullable instancetype)initWithCompletedUnits:(uint64_t)completedUnits
                                      totalUnits:(uint64_t)totalUnits {
    if (completedUnits > totalUnits) {
        return nil;
    }
    self = [super init];
    if (self != nil) {
        _completedUnits = completedUnits;
        _totalUnits = totalUnits;
    }
    return self;
}

@end

@implementation AudioForgeBridgeTimeTransform {
    AudioForgeTimeTransform _transform;
}

- (nullable instancetype)initWithSourceStartFrame:(uint64_t)sourceStartFrame
                                  sourceFrameCount:(uint64_t)sourceFrameCount
                                   outputStartFrame:(uint64_t)outputStartFrame
                                  outputFrameCount:(uint64_t)outputFrameCount {
    if (sourceFrameCount == 0 || outputFrameCount == 0 ||
        sourceStartFrame > UINT64_MAX - sourceFrameCount ||
        outputStartFrame > UINT64_MAX - outputFrameCount) {
        return nil;
    }
    self = [super init];
    if (self != nil) {
        _sourceStartFrame = sourceStartFrame;
        _sourceFrameCount = sourceFrameCount;
        _outputStartFrame = outputStartFrame;
        _outputFrameCount = outputFrameCount;
        _transform = (AudioForgeTimeTransform){
            AUDIOFORGE_TIME_TRANSFORM_STRUCT_SIZE,
            0,
            sourceStartFrame,
            sourceFrameCount,
            outputStartFrame,
            outputFrameCount,
        };
    }
    return self;
}

- (BOOL)mapSourceFrame:(uint64_t)sourceFrame
           outputFrame:(uint64_t *)outputFrame {
    return audioforge_time_transform_map_source_frame(
               &_transform, sourceFrame, outputFrame) == AUDIOFORGE_STATUS_OK;
}

@end

@interface AudioForgeBridgeState ()

- (instancetype)initWithEngineVersion:(NSString *)engineVersion
                           engineReady:(BOOL)engineReady
                   processingSupported:(BOOL)processingSupported
                         projectLoaded:(BOOL)projectLoaded
    NS_DESIGNATED_INITIALIZER;

@end

@implementation AudioForgeBridgeState

- (instancetype)initWithEngineVersion:(NSString *)engineVersion
                           engineReady:(BOOL)engineReady
                   processingSupported:(BOOL)processingSupported
                         projectLoaded:(BOOL)projectLoaded {
    self = [super init];
    if (self != nil) {
        _engineVersion = [engineVersion copy];
        _engineReady = engineReady;
        _processingSupported = processingSupported;
        _projectLoaded = projectLoaded;
    }
    return self;
}

@end

@implementation AudioForgeBridge

+ (NSString *)engineVersion {
    AudioForgeVersion version = audioforge_engine_get_version();
    return [NSString stringWithFormat:@"%u.%u.%u", version.major, version.minor,
                                      version.patch];
}

+ (AudioForgeBridgeState *)currentState {
    AudioForgeEngine *engine = nullptr;
    const AudioForgeStatus engineStatus = audioforge_engine_create(&engine);
    audioforge_engine_destroy(engine);

    AudioForgeProcessRequest request = AUDIOFORGE_PROCESS_REQUEST_INIT;
    AudioForgeProcessResult result = AUDIOFORGE_PROCESS_RESULT_INIT;
    AudioForgeError error = AUDIOFORGE_ERROR_INIT;
    const AudioForgeStatus processingStatus =
        audioforge_engine_process(&request, &result, &error);

    return [[AudioForgeBridgeState alloc]
        initWithEngineVersion:[self engineVersion]
                  engineReady:engineStatus == AUDIOFORGE_STATUS_OK
          processingSupported:processingStatus == AUDIOFORGE_STATUS_OK &&
                               result.status == AUDIOFORGE_STATUS_OK
                projectLoaded:NO];
}

+ (BOOL)supportsEngineAPIMajor:(uint32_t)major
                        minor:(uint32_t)minor
                        patch:(uint32_t)patch {
    const AudioForgeVersion version = audioforge_engine_get_version();
    return major == version.major && minor == version.minor &&
           patch == version.patch;
}

+ (NSError *)errorForEngineErrorCode:(AudioForgeBridgeErrorCode)code {
    NSString *description = @"Unknown engine error.";
    switch (code) {
        case AudioForgeBridgeErrorCodeInvalidArgument:
            description = @"The engine request contains invalid arguments.";
            break;
        case AudioForgeBridgeErrorCodeVersionMismatch:
            description = @"The engine API version is not supported.";
            break;
        case AudioForgeBridgeErrorCodeUnsupportedProcessing:
            description = @"Audio processing is not implemented yet.";
            break;
        case AudioForgeBridgeErrorCodeInvalidCancellationToken:
            description = @"The cancellation token is invalid.";
            break;
        case AudioForgeBridgeErrorCodeInvalidTimeTransform:
            description = @"The time transform is invalid.";
            break;
    }
    return [NSError errorWithDomain:AudioForgeBridgeErrorDomain
                               code:code
                           userInfo:@{NSLocalizedDescriptionKey : description}];
}

@end
