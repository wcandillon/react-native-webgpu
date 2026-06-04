#include "AppleVideoPlayer.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

#include <stdexcept>

namespace rnwgpu {

namespace {

// 3x4 row-major matrices mapping [Y, Cb, Cr, 1] to gamma-encoded R'G'B' (NOT
// linear): YCbCr is derived from gamma-encoded RGB, so this conversion stays in
// the encoded domain. The sRGB decode in srcTransferFunctionParameters
// linearizes afterward (see GPUExternalTexture.cpp). Limited-range (video
// range) means luma is 16..235, chroma is 16..240 (8-bit).
// Reference: https://en.wikipedia.org/wiki/YCbCr (BT.601 / BT.709).
static constexpr float kBT709LimitedToRgb[12] = {
    1.164383f, 0.000000f, 1.792741f, -0.972945f, //
    1.164383f, -0.213249f, -0.532909f, 0.301517f, //
    1.164383f, 2.112402f, 0.000000f, -1.133402f, //
};
static constexpr float kBT601LimitedToRgb[12] = {
    1.164383f, 0.000000f, 1.596027f, -0.874202f, //
    1.164383f, -0.391762f, -0.812968f, 0.531668f, //
    1.164383f, 2.017232f, 0.000000f, -1.085631f, //
};
static constexpr float kBT2020LimitedToRgb[12] = {
    1.164383f, 0.000000f, 1.678674f, -0.915688f, //
    1.164383f, -0.187326f, -0.650424f, 0.347459f, //
    1.164383f, 2.141772f, 0.000000f, -1.148145f, //
};

// Pick the right YUV→RGB matrix from the pixel buffer's color attachments.
// Falls back to BT.709 limited range (the right call for ≥720p H.264, which
// is what AVPlayer hands us for Big Buck Bunny and most streamed media).
static void fillYuvMatrix(CVPixelBufferRef pixelBuffer, float out[12]) {
  CFTypeRef matrixKey = CVBufferGetAttachment(
      pixelBuffer, kCVImageBufferYCbCrMatrixKey, nullptr);
  const float *src = kBT709LimitedToRgb;
  if (matrixKey) {
    auto matrix = (CFStringRef)matrixKey;
    if (CFEqual(matrix, kCVImageBufferYCbCrMatrix_ITU_R_601_4) ||
        CFEqual(matrix, kCVImageBufferYCbCrMatrix_SMPTE_240M_1995)) {
      src = kBT601LimitedToRgb;
    } else if (CFEqual(matrix, kCVImageBufferYCbCrMatrix_ITU_R_2020)) {
      src = kBT2020LimitedToRgb;
    }
  }
  for (int i = 0; i < 12; ++i) {
    out[i] = src[i];
  }
}

// Map a CVPixelBuffer's pixel format to our VideoPixelFormat enum.
static VideoPixelFormat pixelFormatFromCVPixelBuffer(
    CVPixelBufferRef pixelBuffer) {
  OSType type = CVPixelBufferGetPixelFormatType(pixelBuffer);
  switch (type) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
      return VideoPixelFormat::NV12;
    default:
      return VideoPixelFormat::BGRA8;
  }
}

class AppleVideoPlayer : public IVideoPlayer {
public:
  AppleVideoPlayer(AVPlayer *player, AVPlayerItemVideoOutput *output,
                   id loopObserver)
      : _player(player), _output(output), _loopObserver(loopObserver) {}

  ~AppleVideoPlayer() override {
    if (_loopObserver) {
      [[NSNotificationCenter defaultCenter] removeObserver:_loopObserver];
      _loopObserver = nil;
    }
    [_player pause];
    _player = nil;
    _output = nil;
  }

  VideoFrameHandle copyLatestFrame() override {
    CMTime currentTime = [_output itemTimeForHostTime:CACurrentMediaTime()];
    if (![_output hasNewPixelBufferForItemTime:currentTime]) {
      return {};
    }
    // copyPixelBufferForItemTime returns a +1 retained CVPixelBuffer; we then
    // hand it to wrapCVPixelBuffer which adds another retain. Balance with a
    // CFRelease here so we don't leak.
    CVPixelBufferRef pixelBuffer =
        [_output copyPixelBufferForItemTime:currentTime
                         itemTimeForDisplay:nullptr];
    if (!pixelBuffer) {
      return {};
    }
    try {
      auto handle = wrapCVPixelBuffer(pixelBuffer);
      CFRelease(pixelBuffer);
      return handle;
    } catch (...) {
      CFRelease(pixelBuffer);
      throw;
    }
  }

  void play() override { [_player play]; }
  void pause() override { [_player pause]; }

private:
  AVPlayer *_player;
  AVPlayerItemVideoOutput *_output;
  id _loopObserver;
};

} // namespace

VideoFrameHandle wrapCVPixelBuffer(CVPixelBufferRef pixelBuffer) {
  if (!pixelBuffer) {
    throw std::runtime_error("wrapCVPixelBuffer: pointer is null");
  }
  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  if (!ioSurface) {
    throw std::runtime_error(
        "wrapCVPixelBuffer: pixel buffer is not IOSurface-backed (was the "
        "camera/video pipeline configured for Metal/IOSurface output?)");
  }

  // Retain the pixel buffer so the caller can release theirs immediately.
  CFRetain(pixelBuffer);

  VideoFrameHandle handle;
  handle.handle = (void *)ioSurface;
  handle.width = static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer));
  handle.height = static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer));
  handle.pixelFormat = pixelFormatFromCVPixelBuffer(pixelBuffer);
  if (handle.pixelFormat == VideoPixelFormat::NV12) {
    fillYuvMatrix(pixelBuffer, handle.yuvToRgbMatrix);
  }
  handle.deleter = [pixelBuffer]() { CFRelease(pixelBuffer); };
  return handle;
}

std::unique_ptr<IVideoPlayer>
createAppleVideoPlayer(const std::string &path, VideoPixelFormat format) {
  NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
  NSURL *url;
  if ([nsPath hasPrefix:@"http://"] || [nsPath hasPrefix:@"https://"] ||
      [nsPath hasPrefix:@"file://"]) {
    url = [NSURL URLWithString:nsPath];
  } else {
    url = [NSURL fileURLWithPath:nsPath];
  }

  AVPlayerItem *item = [AVPlayerItem playerItemWithURL:url];
  if (!item) {
    throw std::runtime_error("createAppleVideoPlayer: failed to create "
                             "AVPlayerItem");
  }

  // NV12 (kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) lets us hand the
  // IOSurface straight to Dawn as a multi-planar texture for
  // importExternalTexture. BGRA is the "decode + convert" path for the
  // single-plane SharedTextureMemory demo.
  OSType pixelFormat = format == VideoPixelFormat::NV12
                          ? kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                          : kCVPixelFormatType_32BGRA;
  NSDictionary *outputSettings = @{
    (NSString *)kCVPixelBufferPixelFormatTypeKey : @(pixelFormat),
    (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
    (NSString *)kCVPixelBufferMetalCompatibilityKey : @YES,
  };
  AVPlayerItemVideoOutput *output = [[AVPlayerItemVideoOutput alloc]
      initWithPixelBufferAttributes:outputSettings];
  [item addOutput:output];

  AVPlayer *player = [AVPlayer playerWithPlayerItem:item];
  player.actionAtItemEnd = AVPlayerActionAtItemEndNone;

  // Loop on end-of-stream by seeking back to zero.
  __weak AVPlayer *weakPlayer = player;
  id loopObserver = [[NSNotificationCenter defaultCenter]
      addObserverForName:AVPlayerItemDidPlayToEndTimeNotification
                  object:item
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification * /*note*/) {
                [weakPlayer seekToTime:kCMTimeZero];
                [weakPlayer play];
              }];

  return std::make_unique<AppleVideoPlayer>(player, output, loopObserver);
}

std::string writeAppleTestVideoFile() {
  NSString *tmpDir = NSTemporaryDirectory();
  NSString *outputPath =
      [tmpDir stringByAppendingPathComponent:@"rnwgpu-test-video.mp4"];
  NSURL *outputURL = [NSURL fileURLWithPath:outputPath];

  // If the file already exists, reuse it. This makes the example zero-cost on
  // subsequent runs.
  if ([[NSFileManager defaultManager] fileExistsAtPath:outputPath]) {
    return [outputPath UTF8String];
  }

  NSError *error = nil;
  AVAssetWriter *writer = [AVAssetWriter assetWriterWithURL:outputURL
                                                   fileType:AVFileTypeMPEG4
                                                      error:&error];
  if (error || !writer) {
    throw std::runtime_error(
        std::string("writeTestVideoFile: AVAssetWriter init failed: ") +
        [[error localizedDescription] UTF8String]);
  }

  const int kWidth = 256;
  const int kHeight = 256;
  const int kFps = 30;
  const int kSeconds = 3;
  const int kTotalFrames = kFps * kSeconds;

  NSDictionary *videoSettings = @{
    AVVideoCodecKey : AVVideoCodecTypeH264,
    AVVideoWidthKey : @(kWidth),
    AVVideoHeightKey : @(kHeight),
  };
  AVAssetWriterInput *writerInput =
      [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                         outputSettings:videoSettings];
  writerInput.expectsMediaDataInRealTime = NO;

  NSDictionary *bufferAttrs = @{
    (NSString *)kCVPixelBufferPixelFormatTypeKey :
        @(kCVPixelFormatType_32BGRA),
    (NSString *)kCVPixelBufferWidthKey : @(kWidth),
    (NSString *)kCVPixelBufferHeightKey : @(kHeight),
    (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
  };
  AVAssetWriterInputPixelBufferAdaptor *adaptor =
      [AVAssetWriterInputPixelBufferAdaptor
          assetWriterInputPixelBufferAdaptorWithAssetWriterInput:writerInput
                                     sourcePixelBufferAttributes:bufferAttrs];
  [writer addInput:writerInput];

  if (![writer startWriting]) {
    throw std::runtime_error(
        std::string("writeTestVideoFile: startWriting failed: ") +
        [[writer.error localizedDescription] UTF8String]);
  }
  [writer startSessionAtSourceTime:kCMTimeZero];

  for (int i = 0; i < kTotalFrames; ++i) {
    // Spin briefly if the input is not ready (the adaptor pool fills up).
    while (!writerInput.isReadyForMoreMediaData) {
      [NSThread sleepForTimeInterval:0.005];
    }

    CVPixelBufferRef pixelBuffer = NULL;
    CVReturn err = CVPixelBufferPoolCreatePixelBuffer(
        kCFAllocatorDefault, adaptor.pixelBufferPool, &pixelBuffer);
    if (err != kCVReturnSuccess || !pixelBuffer) {
      throw std::runtime_error("writeTestVideoFile: pool exhausted");
    }

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    uint8_t *base =
        static_cast<uint8_t *>(CVPixelBufferGetBaseAddress(pixelBuffer));
    size_t rowBytes = CVPixelBufferGetBytesPerRow(pixelBuffer);
    // Procedural pattern: scrolling diagonal stripes + per-frame color shift.
    int phase = i * 6;
    for (int y = 0; y < kHeight; ++y) {
      uint8_t *row = base + y * rowBytes;
      for (int x = 0; x < kWidth; ++x) {
        uint8_t r = static_cast<uint8_t>((x + phase) & 0xFF);
        uint8_t g = static_cast<uint8_t>((y + phase * 2) & 0xFF);
        uint8_t b =
            static_cast<uint8_t>(((x + y + phase) & 0x40) ? 220 : 30);
        row[x * 4 + 0] = b;
        row[x * 4 + 1] = g;
        row[x * 4 + 2] = r;
        row[x * 4 + 3] = 0xFF;
      }
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

    CMTime pts = CMTimeMake(i, kFps);
    if (![adaptor appendPixelBuffer:pixelBuffer withPresentationTime:pts]) {
      CFRelease(pixelBuffer);
      throw std::runtime_error(
          std::string("writeTestVideoFile: appendPixelBuffer failed: ") +
          [[writer.error localizedDescription] UTF8String]);
    }
    CFRelease(pixelBuffer);
  }

  [writerInput markAsFinished];
  dispatch_semaphore_t sem = dispatch_semaphore_create(0);
  [writer finishWritingWithCompletionHandler:^{
    dispatch_semaphore_signal(sem);
  }];
  dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

  if (writer.status != AVAssetWriterStatusCompleted) {
    throw std::runtime_error(
        std::string("writeTestVideoFile: writer finished with status ") +
        std::to_string(static_cast<long>(writer.status)) + ": " +
        [[writer.error localizedDescription] UTF8String]);
  }

  return [outputPath UTF8String];
}

} // namespace rnwgpu
