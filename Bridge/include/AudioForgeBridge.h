#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const AudioForgeBridgeErrorDomain;

typedef NS_ENUM(NSInteger, AudioForgeBridgeErrorCode) {
    AudioForgeBridgeErrorCodeInvalidArgument = 1,
    AudioForgeBridgeErrorCodeVersionMismatch = 2,
    AudioForgeBridgeErrorCodeUnsupportedProcessing = 3,
    AudioForgeBridgeErrorCodeInvalidCancellationToken = 4,
    AudioForgeBridgeErrorCodeInvalidTimeTransform = 5,
};

@interface AudioForgeBridgeCancellationToken : NSObject

@property(nonatomic, readonly, getter=isCancelled) BOOL cancelled;
- (void)cancel;

@end

@interface AudioForgeBridgeProgress : NSObject

@property(nonatomic, readonly) uint64_t completedUnits;
@property(nonatomic, readonly) uint64_t totalUnits;

- (nullable instancetype)initWithCompletedUnits:(uint64_t)completedUnits
                                      totalUnits:(uint64_t)totalUnits
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@interface AudioForgeBridgeTimeTransform : NSObject

@property(nonatomic, readonly) uint64_t sourceStartFrame;
@property(nonatomic, readonly) uint64_t sourceFrameCount;
@property(nonatomic, readonly) uint64_t outputStartFrame;
@property(nonatomic, readonly) uint64_t outputFrameCount;

- (nullable instancetype)initWithSourceStartFrame:(uint64_t)sourceStartFrame
                                  sourceFrameCount:(uint64_t)sourceFrameCount
                                   outputStartFrame:(uint64_t)outputStartFrame
                                  outputFrameCount:(uint64_t)outputFrameCount
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (BOOL)mapSourceFrame:(uint64_t)sourceFrame
           outputFrame:(uint64_t * _Nullable)outputFrame;

@end

@interface AudioForgeBridge : NSObject

+ (NSString *)engineVersion;
+ (BOOL)supportsEngineAPIMajor:(uint32_t)major
                        minor:(uint32_t)minor
                        patch:(uint32_t)patch;
+ (NSError *)errorForEngineErrorCode:(AudioForgeBridgeErrorCode)code;

@end

NS_ASSUME_NONNULL_END
