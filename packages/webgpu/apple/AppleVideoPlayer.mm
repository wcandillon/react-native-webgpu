#include "AppleVideoPlayer.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

#include <stdexcept>

namespace rnwgpu {

namespace {

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
    CVPixelBufferRef pixelBuffer =
        [_output copyPixelBufferForItemTime:currentTime
                         itemTimeForDisplay:nullptr];
    if (!pixelBuffer) {
      return {};
    }

    IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
    if (!ioSurface) {
      CFRelease(pixelBuffer);
      return {};
    }

    VideoFrameHandle handle;
    handle.handle = (void *)ioSurface;
    handle.width = static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer));
    handle.height = static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer));
    handle.deleter = [pixelBuffer]() { CFRelease(pixelBuffer); };
    return handle;
  }

  void play() override { [_player play]; }
  void pause() override { [_player pause]; }

private:
  AVPlayer *_player;
  AVPlayerItemVideoOutput *_output;
  id _loopObserver;
};

} // namespace

std::unique_ptr<IVideoPlayer>
createAppleVideoPlayer(const std::string &path) {
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

  NSDictionary *outputSettings = @{
    (NSString *)kCVPixelBufferPixelFormatTypeKey :
        @(kCVPixelFormatType_32BGRA),
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
