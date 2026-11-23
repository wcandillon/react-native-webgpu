#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <IOSurface/IOSurfaceRef.h>
#if TARGET_OS_IOS
#import <React/RCTUtils.h>
#import <UIKit/UIKit.h>
#endif

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "NativeBufferViewRegistry.h"
#include "NativeVideoWriter.h"

namespace rnwgpu {

namespace {

std::string describe(NSError *error) {
  if (error == nil) {
    return "unknown error";
  }
  std::string text = std::string(error.localizedDescription.UTF8String ?: "") +
                     " (" + std::string(error.domain.UTF8String ?: "") + " " +
                     std::to_string(error.code) + ")";
  // AVFoundation reports an encoder failure as AVErrorUnknown (-11800, "The
  // operation could not be completed") and puts the code that says what
  // failed, VideoToolbox's or the system's, in NSUnderlyingErrorKey.
  NSError *underlying = error.userInfo[NSUnderlyingErrorKey];
  if (underlying != nil) {
    text += ": " + describe(underlying);
  }
  return text;
}

// Whether the system took the encoder away from the app: iOS interrupts a
// writer when the app leaves the foreground. macOS has no such error.
bool interrupted(NSError *error) {
#if TARGET_OS_IOS
  return error.code == AVErrorOperationInterrupted;
#else
  (void)error;
  return false;
#endif
}

// How long the encoder may take to accept a frame, or a frame's GPU work to
// finish, before the writer gives up.
constexpr auto kTimeout = std::chrono::seconds(20);

// The buffers handed out and not yet encoded: the encoder's pool grows to
// this many, and dequeueBuffer waits for one to come back past it.
constexpr int kMaxBuffersInFlight = 3;

// Presentation times on the 90 kHz clock of MPEG.
CMTime timeAt(double seconds) {
  return CMTimeMake(std::llround(seconds * 90000.0), 90000);
}

// Everything the writer's queue and the JS thread share. Held by every block
// the writer dispatches, so that it outlives a writer the JS wrapper dropped.
struct State {
  VideoWriterOptions options;
  AVAssetWriter *writer = nil;
  AVAssetWriterInput *videoInput = nil;
  AVAssetWriterInputPixelBufferAdaptor *adaptor = nil;
  AVAssetReader *audioReader = nil;
  AVAssetReaderTrackOutput *audioOutput = nil;
  AVAssetWriterInput *audioInput = nil;
  dispatch_queue_t queue = nil;
  // Whether the file at the path is this writer's to delete: from when the
  // writer opens it until it is deleted, or written out.
  bool ownsFile = false;
  // The file the writer made, once it exists: a file another writer made at
  // the same path since is not deleted.
  std::optional<std::pair<dev_t, ino_t>> fileId;

  std::mutex mutex;
  // The buffers dequeued and not yet appended, by IOSurface, each retaining
  // its CVPixelBuffer.
  std::unordered_map<IOSurfaceRef, CVPixelBufferRef> outstanding;
  // Dequeued or appended, not yet in the encoder.
  int inFlight = 0;
  struct PendingDequeue {
    std::function<void(VideoFrameHandle)> onBuffer;
    VideoWriterErrorCallback onError;
  };
  std::deque<PendingDequeue> pendingDequeues;
  std::optional<std::pair<std::string, std::string>> error;
  bool finishing = false;
  double lastTimestamp = -std::numeric_limits<double>::infinity();
  int64_t frames = 0;

  // The sound, written on the queue alongside the video and never ahead of
  // it: the next sample read and not yet written, and whether it ended.
  CMSampleBufferRef audioSample = nullptr;
  // The first packet of audioSample not written yet.
  CMItemCount audioSampleStart = 0;
  bool audioFinished = false;

#if TARGET_OS_IOS
  // Observes the app going to the background: see observeBackground().
  id backgroundObserver = nil;
#endif

  ~State() {
#if TARGET_OS_IOS
    if (backgroundObserver != nil) {
      [NSNotificationCenter.defaultCenter removeObserver:backgroundObserver];
    }
#endif
    for (auto &entry : outstanding) {
      CVPixelBufferRelease(entry.second);
    }
    if (audioSample != nullptr) {
      CFRelease(audioSample);
    }
  }

  // Writes the sound up to `until` seconds (the start of the video frame
  // about to be written, or the end of the video): the writer interleaves the
  // tracks and holds the video back while the sound is behind. Runs on the
  // queue. Returns false, with the error recorded, once it failed.
  bool writeAudio(double until, bool last) {
    while (audioInput != nil && !audioFinished) {
      if (audioSample == nullptr) {
        audioSample = [audioOutput copyNextSampleBuffer];
        audioSampleStart = 0;
        if (audioSample == nullptr) {
          if (audioReader.status == AVAssetReaderStatusFailed) {
            std::lock_guard<std::mutex> lock(mutex);
            failLocked("audio-failed",
                       "VideoWriter: reading the sound failed: " +
                           describe(audioReader.error));
            return false;
          }
          [audioInput markAsFinished];
          audioFinished = true;
          return true;
        }
      }
      // While frames come, the sound is written as far as the writer takes
      // it: it holds the sound back when it leads the video by more than its
      // interleaving period, which frames far apart in time (a slideshow)
      // do. The rest waits for the next frame. Only the end waits for it.
      if (!last && !audioInput.readyForMoreMediaData) {
        return true;
      }
      // A sample buffer holds many packets (all of an AAC file's, copied as
      // they are): the ones from `until` on wait for the video, or are cut
      // past its end. The buffer is kept whole and written a range at a
      // time, from audioSampleStart.
      CMItemCount count = CMSampleBufferGetNumSamples(audioSample);
      CMItemCount kept = audioSampleStart;
      CMSampleTimingInfo timing;
      while (kept < count &&
             CMSampleBufferGetSampleTimingInfo(audioSample, kept, &timing) ==
                 noErr &&
             CMTimeGetSeconds(timing.presentationTimeStamp) < until) {
        kept++;
      }
      if (kept == audioSampleStart) {
        if (last) {
          [audioInput markAsFinished];
          audioFinished = true;
          [audioReader cancelReading];
        }
        return true;
      }
      CMSampleBufferRef head = audioSample;
      if (audioSampleStart > 0 || kept < count) {
        head = nullptr;
        if (CMSampleBufferCopySampleBufferForRange(
                kCFAllocatorDefault, audioSample,
                CFRangeMake(audioSampleStart, kept - audioSampleStart),
                &head) != noErr) {
          std::lock_guard<std::mutex> lock(mutex);
          failLocked("audio-failed", "VideoWriter: cannot split the sound");
          return false;
        }
      } else {
        CFRetain(head);
      }
      if (kept < count) {
        audioSampleStart = kept;
      } else {
        CFRelease(audioSample);
        audioSample = nullptr;
      }
      if (!waitUntil([&]() { return audioInput.readyForMoreMediaData; },
                     "the encoder to take the sound")) {
        CFRelease(head);
        return false;
      }
      BOOL appended = [audioInput appendSampleBuffer:head];
      CFRelease(head);
      if (!appended) {
        std::lock_guard<std::mutex> lock(mutex);
        failLocked("audio-failed", "VideoWriter: writing the sound failed: " +
                                       describe(writer.error));
        return false;
      }
    }
    return true;
  }

  // Lets go of AVFoundation's objects once the file is written or given up:
  // the writer deletes its temporary files only when it is deallocated,
  // which the JS object holding this state would otherwise delay until it
  // is garbage collected. Runs on the queue.
  void releaseWriter() {
    if (audioSample != nullptr) {
      CFRelease(audioSample);
      audioSample = nullptr;
    }
    adaptor = nil;
    videoInput = nil;
    audioInput = nil;
    audioOutput = nil;
    audioReader = nil;
    writer = nil;
  }

  // Records the first failure. Called with the mutex held.
  void failLocked(const std::string &code, const std::string &message) {
    if (!error.has_value()) {
      error = std::make_pair(code, message);
    }
  }

  // Notes which file the writer made, once it exists. Called with the mutex
  // held.
  void noteFileLocked() {
    struct stat info;
    if (ownsFile && !fileId.has_value() &&
        stat(options.path.c_str(), &info) == 0) {
      fileId = std::make_pair(info.st_dev, info.st_ino);
    }
  }

  // Deletes the partial file, once, and only the one this writer made.
  // Called with the mutex held.
  void deleteFileLocked() {
    noteFileLocked();
    if (!ownsFile) {
      return;
    }
    ownsFile = false;
    struct stat info;
    if (stat(options.path.c_str(), &info) != 0) {
      return;
    }
    if (fileId.has_value() &&
        (info.st_dev != fileId->first || info.st_ino != fileId->second)) {
      return;
    }
    unlink(options.path.c_str());
  }

  // Hands out a buffer from the encoder's pool. Called with the mutex held,
  // with room in flight.
  VideoFrameHandle takeBufferLocked(const std::shared_ptr<State> &self) {
    CVPixelBufferRef pixelBuffer = nullptr;
    CVReturn status = CVPixelBufferPoolCreatePixelBuffer(
        nullptr, adaptor.pixelBufferPool, &pixelBuffer);
    if (status != kCVReturnSuccess || pixelBuffer == nullptr) {
      throw VideoWriterError(
          "encoder-failed",
          "VideoWriter: no buffer from the encoder's pool (CVReturn " +
              std::to_string(status) + ")");
    }
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixelBuffer);
    if (surface == nullptr) {
      CVPixelBufferRelease(pixelBuffer);
      throw VideoWriterError(
          "unsupported",
          "VideoWriter: the encoder's buffers have no IOSurface");
    }
    // One reference for the writer (until appended), one for the handle.
    outstanding[surface] = pixelBuffer;
    inFlight++;
    CVPixelBufferRetain(pixelBuffer);
    VideoFrameHandle handle;
    handle.handle = surface;
    handle.width = options.width;
    handle.height = options.height;
    handle.pixelFormat = VideoPixelFormat::BGRA8;
    std::weak_ptr<State> weak = self;
    handle.deleter = [weak, surface, pixelBuffer]() {
      // A buffer released without being appended goes back unencoded.
      if (auto state = weak.lock()) {
        std::unique_lock<std::mutex> lock(state->mutex);
        auto found = state->outstanding.find(surface);
        if (found != state->outstanding.end() && found->second == pixelBuffer) {
          CVPixelBufferRelease(found->second);
          state->outstanding.erase(found);
          state->inFlight--;
          state->serveLocked(state, lock);
        }
      }
      CVPixelBufferRelease(pixelBuffer);
    };
    return handle;
  }

  // Answers the dequeues that wait, as far as there is room. Called with the
  // mutex held through `lock`, which it releases around the callbacks.
  void serveLocked(const std::shared_ptr<State> &self,
                   std::unique_lock<std::mutex> &lock) {
    while (!pendingDequeues.empty()) {
      auto pending = std::move(pendingDequeues.front());
      if (error.has_value()) {
        pendingDequeues.pop_front();
        auto failure = *error;
        lock.unlock();
        pending.onError(failure.first, failure.second);
        lock.lock();
        continue;
      }
      // finish() called: the writer takes no more frames, and lets go of its
      // pool on the queue.
      if (finishing) {
        pendingDequeues.pop_front();
        lock.unlock();
        pending.onError("invalid-argument",
                        "VideoWriter.dequeueBuffer(): the writer is finishing");
        lock.lock();
        continue;
      }
      if (inFlight >= kMaxBuffersInFlight) {
        pendingDequeues.front() = std::move(pending);
        return;
      }
      pendingDequeues.pop_front();
      VideoFrameHandle handle;
      try {
        handle = takeBufferLocked(self);
      } catch (const VideoWriterError &e) {
        failLocked(e.code(), e.what());
        auto failure = *error;
        lock.unlock();
        pending.onError(failure.first, failure.second);
        lock.lock();
        continue;
      }
      lock.unlock();
      pending.onBuffer(std::move(handle));
      lock.lock();
    }
  }

  // Waits for `ready`, polling: AVAssetWriter, without real-time input,
  // makes its caller wait this way. Returns false (with the error recorded)
  // once the writer failed or the time ran out.
  template <typename Ready> bool waitUntil(Ready ready, const char *what) {
    auto deadline = std::chrono::steady_clock::now() + kTimeout;
    while (!ready()) {
      if (writer.status != AVAssetWriterStatusWriting) {
        std::lock_guard<std::mutex> lock(mutex);
        // The system took the encoder away (the app left the foreground).
        failLocked(interrupted(writer.error) ? "interrupted" : "encoder-failed",
                   "VideoWriter: the encoder stopped: " +
                       describe(writer.error));
        return false;
      }
      if (std::chrono::steady_clock::now() > deadline) {
        std::lock_guard<std::mutex> lock(mutex);
        failLocked("encoder-failed",
                   std::string("VideoWriter: timed out waiting for ") + what);
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
  }
};

class AppleVideoWriter : public IVideoWriter {
public:
  explicit AppleVideoWriter(const VideoWriterOptions &options)
      : _state(std::make_shared<State>()) {
    _state->options = options;
    @autoreleasepool {
      start();
    }
  }

  ~AppleVideoWriter() override {
    // Dropped without finish(): the partial file goes.
    if (!_done) {
      cancel(nullptr);
    }
  }

  void dequeueBuffer(std::function<void(VideoFrameHandle)> onBuffer,
                     VideoWriterErrorCallback onError) override {
    std::unique_lock<std::mutex> lock(_state->mutex);
    if (_state->finishing && !_state->error.has_value()) {
      lock.unlock();
      onError("invalid-argument",
              "VideoWriter.dequeueBuffer(): the writer is finishing");
      return;
    }
    _state->pendingDequeues.push_back(
        {std::move(onBuffer), std::move(onError)});
    @autoreleasepool {
      _state->serveLocked(_state, lock);
    }
  }

  void append(void *buffer, double timestamp, wgpu::SharedFence fence,
              uint64_t signaledValue) override {
    CVPixelBufferRef pixelBuffer = nullptr;
    {
      std::lock_guard<std::mutex> lock(_state->mutex);
      if (_state->error.has_value()) {
        throw VideoWriterError(_state->error->first, _state->error->second);
      }
      if (_state->finishing) {
        throw VideoWriterError("invalid-argument",
                               "VideoWriter.append(): the writer is finishing");
      }
      auto found = _state->outstanding.find(static_cast<IOSurfaceRef>(buffer));
      if (found == _state->outstanding.end()) {
        throw VideoWriterError(
            "invalid-argument",
            "VideoWriter.append(): the buffer was not dequeued from this "
            "writer, or was appended already");
      }
      if (!std::isfinite(timestamp) || timestamp < 0 ||
          timestamp <= _state->lastTimestamp) {
        throw VideoWriterError(
            "invalid-argument",
            "VideoWriter.append(): the timestamp must be finite, positive, "
            "and greater than the previous frame's");
      }
      _state->lastTimestamp = timestamp;
      pixelBuffer = found->second;
      _state->outstanding.erase(found);
    }
    auto state = _state;
    dispatch_async(_state->queue, ^{
      @autoreleasepool {
        appendOnQueue(state, pixelBuffer, timestamp, fence, signaledValue);
      }
    });
  }

  void finish(std::function<void()> onDone,
              VideoWriterErrorCallback onError) override {
    {
      std::unique_lock<std::mutex> lock(_state->mutex);
      if (_done) {
        auto failure = _state->error.value_or(std::make_pair(
            std::string("invalid-argument"),
            std::string("VideoWriter.finish(): the writer is finishing "
                        "already")));
        lock.unlock();
        onError(failure.first, failure.second);
        return;
      }
      _state->finishing = true;
      // The dequeues still waiting get no buffer.
      @autoreleasepool {
        _state->serveLocked(_state, lock);
      }
    }
    _done = true;
    auto state = _state;
#if TARGET_OS_IOS
    // Every frame is in: the writer only has to write them out and close the
    // file, which it may finish in the background if the app leaves now.
    UIApplication *application = RCTSharedApplication();
    __block UIBackgroundTaskIdentifier task = UIBackgroundTaskInvalid;
    if (application != nil) {
      task = [application beginBackgroundTaskWithName:@"VideoWriter.finish"
                                    expirationHandler:^{
                                      [application endBackgroundTask:task];
                                      task = UIBackgroundTaskInvalid;
                                    }];
    }
#endif
    dispatch_async(_state->queue, ^{
      @autoreleasepool {
        auto failure = finishOnQueue(state);
        if (failure.has_value()) {
          cleanUp(state);
          onError(failure->first, failure->second);
        } else {
          {
            // Written out: the file is the app's now.
            std::lock_guard<std::mutex> lock(state->mutex);
            state->ownsFile = false;
          }
          state->releaseWriter();
          onDone();
        }
#if TARGET_OS_IOS
        dispatch_async(dispatch_get_main_queue(), ^{
          if (task != UIBackgroundTaskInvalid) {
            [application endBackgroundTask:task];
            task = UIBackgroundTaskInvalid;
          }
        });
#endif
      }
    });
  }

  void cancel(std::function<void()> onDone) noexcept override {
    {
      std::unique_lock<std::mutex> lock(_state->mutex);
      _state->failLocked("cancelled", "VideoWriter: cancelled");
      _state->finishing = true;
      @autoreleasepool {
        _state->serveLocked(_state, lock);
      }
    }
    auto state = _state;
    // A finish() running on the queue already fails on its own.
    if (!_done) {
      _done = true;
      dispatch_async(_state->queue, ^{
        @autoreleasepool {
          cleanUp(state);
        }
      });
    }
    // Done once what the queue holds is: the file is gone by then.
    if (onDone) {
      dispatch_async(_state->queue, ^{
        onDone();
      });
    }
  }

private:
  void start() {
    auto &options = _state->options;
    NSString *path = [NSString stringWithUTF8String:options.path.c_str()];
    if (path == nil) {
      throw VideoWriterError("invalid-argument",
                             "VideoWriter: the path is not valid UTF-8");
    }
    NSURL *url = [NSURL fileURLWithPath:path];
    NSFileManager *files = [NSFileManager defaultManager];
    BOOL isDirectory = NO;
    if (![files fileExistsAtPath:path.stringByDeletingLastPathComponent
                     isDirectory:&isDirectory] ||
        !isDirectory) {
      throw VideoWriterError("write-failed", "VideoWriter: the directory of " +
                                                 options.path +
                                                 " does not exist");
    }
    NSError *error = nil;
    AVAssetWriter *writer = [AVAssetWriter assetWriterWithURL:url
                                                     fileType:AVFileTypeMPEG4
                                                        error:&error];
    if (writer == nil) {
      throw VideoWriterError("write-failed", "VideoWriter: cannot write " +
                                                 options.path + ": " +
                                                 describe(error));
    }
    writer.shouldOptimizeForNetworkUse = YES;
    _state->writer = writer;

    NSDictionary *compression = @{
      AVVideoAverageBitRateKey : @(options.bitrate),
      AVVideoExpectedSourceFrameRateKey : @(options.frameRate),
      AVVideoMaxKeyFrameIntervalKey :
          @(std::max(1L, std::lround(options.frameRate * 2))),
      AVVideoProfileLevelKey : AVVideoProfileLevelH264HighAutoLevel,
    };
    NSDictionary *videoSettings = @{
      AVVideoCodecKey : AVVideoCodecTypeH264,
      AVVideoWidthKey : @(options.width),
      AVVideoHeightKey : @(options.height),
      AVVideoCompressionPropertiesKey : compression,
      // The frames are sRGB: tagged BT.709, which players read as such,
      // rather than left for each to guess.
      AVVideoColorPropertiesKey : @{
        AVVideoColorPrimariesKey : AVVideoColorPrimaries_ITU_R_709_2,
        AVVideoTransferFunctionKey : AVVideoTransferFunction_ITU_R_709_2,
        AVVideoYCbCrMatrixKey : AVVideoYCbCrMatrix_ITU_R_709_2,
      },
    };
    if (![writer canApplyOutputSettings:videoSettings
                           forMediaType:AVMediaTypeVideo]) {
      throw VideoWriterError("unsupported",
                             "VideoWriter: the H.264 encoder does not take " +
                                 std::to_string(options.width) + "x" +
                                 std::to_string(options.height) + " at " +
                                 std::to_string(options.bitrate) + " bps");
    }
    AVAssetWriterInput *videoInput =
        [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                           outputSettings:videoSettings];
    videoInput.expectsMediaDataInRealTime = NO;
    // Fine enough for any frame rate: 29.97 fps is 3003 ticks a frame.
    videoInput.mediaTimeScale = 90000;
    NSDictionary *bufferAttributes = @{
      (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA),
      (id)kCVPixelBufferWidthKey : @(options.width),
      (id)kCVPixelBufferHeightKey : @(options.height),
      // IOSurface-backed, so that a GPU device can import it.
      (id)kCVPixelBufferIOSurfacePropertiesKey : @{},
      (id)kCVPixelBufferMetalCompatibilityKey : @YES,
    };
    _state->adaptor = [AVAssetWriterInputPixelBufferAdaptor
        assetWriterInputPixelBufferAdaptorWithAssetWriterInput:videoInput
                                   sourcePixelBufferAttributes:
                                       bufferAttributes];
    if (![writer canAddInput:videoInput]) {
      throw VideoWriterError("unsupported",
                             "VideoWriter: cannot add the video track");
    }
    [writer addInput:videoInput];
    _state->videoInput = videoInput;

    if (options.audioPath.has_value()) {
      prepareAudio();
    }

    _state->queue =
        dispatch_queue_create("com.webgpu.VideoWriter", DISPATCH_QUEUE_SERIAL);
    // AVAssetWriter refuses a file that exists: it is replaced once nothing
    // else can fail before the writer opens it.
    [files removeItemAtURL:url error:nil];
    _state->ownsFile = true;
    if (![writer startWriting]) {
      auto message = "VideoWriter: cannot write " + options.path + ": " +
                     describe(writer.error);
      [files removeItemAtURL:url error:nil];
      throw VideoWriterError("write-failed", message);
    }
    [writer startSessionAtSourceTime:kCMTimeZero];
    {
      std::lock_guard<std::mutex> lock(_state->mutex);
      _state->noteFileLocked();
    }
    if (_state->audioInput != nil && ![_state->audioReader startReading]) {
      auto message = "VideoWriter: cannot read " + *options.audioPath + ": " +
                     describe(_state->audioReader.error);
      [writer cancelWriting];
      [files removeItemAtURL:url error:nil];
      throw VideoWriterError("audio-failed", message);
    }
    observeBackground();
  }

  // An app in the background cannot use the GPU, nor the hardware encoder:
  // Skia holds the GPU work submitted meanwhile until the app comes back, so
  // the frames' fences would not signal, and the encoder fails an AVAssetWriter
  // writing when the app leaves (AVErrorOperationInterrupted, -11847). The
  // writer fails at once with "interrupted" instead, unless finish() runs
  // already, which completes in a background task. macOS apps keep the GPU.
  void observeBackground() {
#if TARGET_OS_IOS
    std::weak_ptr<State> weak = _state;
    _state->backgroundObserver = [NSNotificationCenter.defaultCenter
        addObserverForName:UIApplicationDidEnterBackgroundNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *) {
                  auto state = weak.lock();
                  if (state == nullptr) {
                    return;
                  }
                  std::unique_lock<std::mutex> lock(state->mutex);
                  if (state->finishing || state->error.has_value()) {
                    return;
                  }
                  state->failLocked("interrupted",
                                    "VideoWriter: the app went to the "
                                    "background");
                  // The calls waiting on the writer reject now: the queue
                  // may be held by a frame whose GPU work only finishes once
                  // the app is back. The file goes once the queue is free.
                  @autoreleasepool {
                    state->serveLocked(state, lock);
                  }
                  dispatch_async(state->queue, ^{
                    @autoreleasepool {
                      cleanUp(state);
                    }
                  });
                }];
#endif
  }

  void prepareAudio() {
    auto &options = _state->options;
    NSString *audioPath =
        [NSString stringWithUTF8String:options.audioPath->c_str()];
    if (audioPath == nil) {
      throw VideoWriterError("invalid-argument",
                             "VideoWriter: the audio path is not valid UTF-8");
    }
    NSURL *url = [NSURL fileURLWithPath:audioPath];
    if (![[NSFileManager defaultManager] fileExistsAtPath:url.path]) {
      throw VideoWriterError("audio-failed", "VideoWriter: no sound file at " +
                                                 *options.audioPath);
    }
    AVURLAsset *asset = [AVURLAsset URLAssetWithURL:url options:nil];
    __block NSArray<AVAssetTrack *> *tracks = nil;
    __block NSError *loadError = nil;
    dispatch_semaphore_t loaded = dispatch_semaphore_create(0);
    [asset loadTracksWithMediaType:AVMediaTypeAudio
                 completionHandler:^(NSArray<AVAssetTrack *> *result,
                                     NSError *error) {
                   tracks = result;
                   loadError = error;
                   dispatch_semaphore_signal(loaded);
                 }];
    dispatch_semaphore_wait(loaded, DISPATCH_TIME_FOREVER);
    if (tracks.count == 0) {
      throw VideoWriterError("audio-failed",
                             "VideoWriter: " + *options.audioPath +
                                 " has no sound track: " + describe(loadError));
    }
    AVAssetTrack *track = tracks.firstObject;
    NSError *error = nil;
    AVAssetReader *reader = [AVAssetReader assetReaderWithAsset:asset
                                                          error:&error];
    if (reader == nil) {
      throw VideoWriterError("audio-failed", "VideoWriter: cannot read " +
                                                 *options.audioPath + ": " +
                                                 describe(error));
    }
    CMFormatDescriptionRef format = nullptr;
    NSArray *formats = track.formatDescriptions;
    if (formats.count > 0) {
      format = (__bridge CMFormatDescriptionRef)formats.firstObject;
    }
    const AudioStreamBasicDescription *asbd =
        format != nullptr
            ? CMAudioFormatDescriptionGetStreamBasicDescription(format)
            : nullptr;
    const bool isAAC = format != nullptr && CMFormatDescriptionGetMediaSubType(
                                                format) == kAudioFormatMPEG4AAC;
    AVAssetReaderTrackOutput *output = nil;
    AVAssetWriterInput *input = nil;
    if (isAAC) {
      // AAC already: the packets are copied as they are.
      output = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:track
                                                          outputSettings:nil];
      input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeAudio
                                                 outputSettings:nil
                                               sourceFormatHint:format];
    } else {
      // MP3 or anything else AVFoundation reads: decoded, encoded to AAC.
      output = [AVAssetReaderTrackOutput
          assetReaderTrackOutputWithTrack:track
                           outputSettings:@{
                             AVFormatIDKey : @(kAudioFormatLinearPCM)
                           }];
      double sampleRate =
          asbd != nullptr && asbd->mSampleRate > 0 ? asbd->mSampleRate : 44100;
      int channels = asbd != nullptr && asbd->mChannelsPerFrame > 0
                         ? std::min<int>(asbd->mChannelsPerFrame, 2)
                         : 2;
      input = [AVAssetWriterInput
          assetWriterInputWithMediaType:AVMediaTypeAudio
                         outputSettings:@{
                           AVFormatIDKey : @(kAudioFormatMPEG4AAC),
                           AVSampleRateKey : @(sampleRate),
                           AVNumberOfChannelsKey : @(channels),
                           AVEncoderBitRateKey : @(128000 * channels / 2),
                         }];
    }
    output.alwaysCopiesSampleData = NO;
    if (![reader canAddOutput:output] || ![_state->writer canAddInput:input]) {
      throw VideoWriterError("audio-failed",
                             "VideoWriter: the sound of " + *options.audioPath +
                                 " cannot be written to an .mp4 file");
    }
    [reader addOutput:output];
    input.expectsMediaDataInRealTime = NO;
    [_state->writer addInput:input];
    _state->audioReader = reader;
    _state->audioOutput = output;
    _state->audioInput = input;
  }

  static void appendOnQueue(const std::shared_ptr<State> &state,
                            CVPixelBufferRef pixelBuffer, double timestamp,
                            wgpu::SharedFence fence, uint64_t signaledValue) {
    auto release = [&]() {
      CVPixelBufferRelease(pixelBuffer);
      std::unique_lock<std::mutex> lock(state->mutex);
      state->inFlight--;
      state->serveLocked(state, lock);
    };
    // Failed or cancelled: the frame is dropped.
    bool failed = false;
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      failed = state->error.has_value();
    }
    if (!failed && fence) {
      // The encoder reads the buffer as soon as it has it: the GPU must be
      // done drawing into it first.
      dispatch_semaphore_t signaled = dispatch_semaphore_create(0);
      auto ok = std::make_shared<std::atomic<bool>>(false);
      waitForSharedFence(std::move(fence), signaledValue,
                         [signaled, ok](bool success) {
                           ok->store(success);
                           dispatch_semaphore_signal(signaled);
                         });
      dispatch_semaphore_wait(signaled, DISPATCH_TIME_FOREVER);
      if (!ok->load()) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->failLocked("encoder-failed",
                          "VideoWriter: the GPU work drawing a frame never "
                          "finished");
        failed = true;
      }
    }
    if (!failed) {
      failed = !state->writeAudio(timestamp, false);
    }
    if (!failed) {
      // Appending while the input is not ready, or once the writer stopped
      // (the app went to the background), raises: checked right before.
      // The writer interleaves the tracks, and holds the video back until
      // the sound is ahead of it by its interleaving period (about half a
      // second): the sound is fed ahead, a step at a time, while the video
      // waits, or neither moves. What goes past the end of the video is cut
      // by endSessionAtSourceTime in finish().
      constexpr double kAudioAheadStep = 0.1;
      double audioUntil = timestamp;
      bool audioFailed = false;
      failed = !state->waitUntil(
          [&]() {
            if (state->videoInput.readyForMoreMediaData) {
              return true;
            }
            if (state->audioInput != nil && !state->audioFinished &&
                state->audioInput.readyForMoreMediaData) {
              audioUntil += kAudioAheadStep;
              audioFailed = !state->writeAudio(audioUntil, false);
            }
            return audioFailed;
          },
          "the encoder to take a frame");
      failed = failed || audioFailed;
    }
    if (!failed) {
      if ([state->adaptor appendPixelBuffer:pixelBuffer
                       withPresentationTime:timeAt(timestamp)]) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->frames++;
      } else {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->failLocked("write-failed",
                          "VideoWriter: writing a frame failed: " +
                              describe(state->writer.error));
        failed = true;
      }
    }
    // A failure deletes the partial file before the calls waiting on the
    // writer reject with it.
    bool cancelled = false;
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      cancelled =
          state->error.has_value() && state->error->first == "cancelled";
    }
    if (failed && !cancelled) {
      cleanUp(state);
    }
    release();
  }

  // Runs on the queue after every append: returns the failure, if any.
  static std::optional<std::pair<std::string, std::string>>
  finishOnQueue(const std::shared_ptr<State> &state) {
    auto stopped = [&]() {
      return std::make_pair(std::string(interrupted(state->writer.error)
                                            ? "interrupted"
                                            : "write-failed"),
                            "VideoWriter: writing " + state->options.path +
                                " stopped: " + describe(state->writer.error));
    };
    double end = 0;
    int64_t frames = 0;
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      if (state->error.has_value()) {
        return state->error;
      }
      frames = state->frames;
      // The last frame lasts one frame at the nominal rate.
      end = state->lastTimestamp + 1.0 / state->options.frameRate;
    }
    if (frames == 0) {
      return std::make_pair(std::string("invalid-argument"),
                            std::string("VideoWriter.finish(): no frame was "
                                        "appended"));
    }
    if (state->writer.status != AVAssetWriterStatusWriting) {
      return stopped();
    }
    [state->videoInput markAsFinished];
    // The sound left until the video ends, and a little past it: the packet
    // the end falls in can be timestamped after it (by the encoder delay the
    // writer adds), and what is past the end is cut by the edit list
    // endSessionAtSourceTime writes.
    constexpr double kAudioPastEnd = 0.1;
    if (!state->writeAudio(end + kAudioPastEnd, true)) {
      std::lock_guard<std::mutex> lock(state->mutex);
      return state->error;
    }
    if (state->writer.status != AVAssetWriterStatusWriting) {
      return stopped();
    }
    [state->writer endSessionAtSourceTime:timeAt(end)];
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    [state->writer finishWritingWithCompletionHandler:^{
      dispatch_semaphore_signal(done);
    }];
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    if (state->writer.status != AVAssetWriterStatusCompleted) {
      return std::make_pair(std::string("write-failed"),
                            "VideoWriter: finishing " + state->options.path +
                                " failed: " + describe(state->writer.error));
    }
    return std::nullopt;
  }

  // Stops the sound and the writer, and deletes the partial file.
  static void cleanUp(const std::shared_ptr<State> &state) {
    if (state->audioReader != nil &&
        state->audioReader.status == AVAssetReaderStatusReading) {
      [state->audioReader cancelReading];
    }
    if (state->writer != nil &&
        state->writer.status == AVAssetWriterStatusWriting) {
      [state->writer cancelWriting];
    }
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->deleteFileLocked();
    }
    state->releaseWriter();
  }

  std::shared_ptr<State> _state;
  bool _done = false;
};

} // namespace

std::unique_ptr<IVideoWriter>
createAppleVideoWriter(const VideoWriterOptions &options) {
  return std::make_unique<AppleVideoWriter>(options);
}

} // namespace rnwgpu
