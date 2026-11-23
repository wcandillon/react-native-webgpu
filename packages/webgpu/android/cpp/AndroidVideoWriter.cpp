#include <android/api-level.h>
#include <android/hardware_buffer.h>
#include <android/native_window.h>
#include <fcntl.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "NativeFrameImport.h"
#include "NativeVideoWriter.h"
#include "WebGPURuntime.h"

namespace rnwgpu {

namespace {

constexpr const char *kVideoMime = "video/avc";
constexpr const char *kAacMime = "audio/mp4a-latm";
// MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface
constexpr int32_t kColorFormatSurface = 0x7F000789;
// MediaCodec.BUFFER_FLAG_KEY_FRAME (AMEDIACODEC_BUFFER_FLAG_KEY_FRAME is only
// declared from API level 34).
constexpr uint32_t kKeyFrameFlag = 1;
constexpr int64_t kDequeueTimeoutUs = 10'000;
// How long the encoder may go without output before the writer gives up.
constexpr auto kTimeout = std::chrono::seconds(20);
// The buffers the writer hands out: each is copied into the encoder's input
// surface as it is appended, and free again right after.
constexpr size_t kBufferCount = 3;

// An encoded sample of the sound, muxed alongside the video.
struct AudioSample {
  std::vector<uint8_t> data;
  int64_t timeUs = 0;
  uint32_t flags = 0;
};

struct MediaFormatDeleter {
  void operator()(AMediaFormat *format) const { AMediaFormat_delete(format); }
};
using MediaFormat = std::unique_ptr<AMediaFormat, MediaFormatDeleter>;

struct MediaCodecDeleter {
  void operator()(AMediaCodec *codec) const {
    AMediaCodec_stop(codec);
    AMediaCodec_delete(codec);
  }
};
using MediaCodec = std::unique_ptr<AMediaCodec, MediaCodecDeleter>;

struct MediaExtractorDeleter {
  void operator()(AMediaExtractor *extractor) const {
    AMediaExtractor_delete(extractor);
  }
};
using MediaExtractor = std::unique_ptr<AMediaExtractor, MediaExtractorDeleter>;

std::string mimeOf(AMediaFormat *format) {
  const char *mime = nullptr;
  AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
  return mime != nullptr ? mime : "";
}

// Reads the sound of `path` as AAC samples: copied when the file holds AAC
// already, decoded and encoded otherwise (MP3, say). Returns the format of the
// track to mux them as. Throws VideoWriterError("audio-failed"). The whole
// file is read: the writer cuts it where the video ends.
class AudioReader {
public:
  explicit AudioReader(std::string path) : _path(std::move(path)) {}

  MediaFormat read(std::vector<AudioSample> &samples,
                   const std::atomic<bool> &stop) {
    int fd = open(_path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
      fail("cannot open " + _path + ": " + std::strerror(errno));
    }
    struct stat info;
    fstat(fd, &info);
    MediaExtractor extractor(AMediaExtractor_new());
    auto status =
        AMediaExtractor_setDataSourceFd(extractor.get(), fd, 0, info.st_size);
    close(fd);
    if (status != AMEDIA_OK) {
      fail(_path + " is not a sound file Android reads (" +
           std::to_string(status) + ")");
    }
    MediaFormat trackFormat;
    for (size_t i = 0; i < AMediaExtractor_getTrackCount(extractor.get());
         i++) {
      MediaFormat format(AMediaExtractor_getTrackFormat(extractor.get(), i));
      if (mimeOf(format.get()).rfind("audio/", 0) == 0) {
        AMediaExtractor_selectTrack(extractor.get(), i);
        trackFormat = std::move(format);
        break;
      }
    }
    if (trackFormat == nullptr) {
      fail(_path + " has no sound track");
    }
    if (mimeOf(trackFormat.get()) == kAacMime) {
      copy(extractor.get(), samples, stop);
      return trackFormat;
    }
    return transcode(extractor.get(), trackFormat.get(), samples, stop);
  }

private:
  [[noreturn]] void fail(const std::string &message) {
    throw VideoWriterError("audio-failed", "VideoWriter: " + message);
  }

  void copy(AMediaExtractor *extractor, std::vector<AudioSample> &samples,
            const std::atomic<bool> &stop) {
    std::vector<uint8_t> buffer(1 << 20);
    while (!stop.load()) {
      auto size = AMediaExtractor_readSampleData(extractor, buffer.data(),
                                                 buffer.size());
      if (size < 0) {
        break;
      }
      AudioSample sample;
      sample.data.assign(buffer.begin(), buffer.begin() + size);
      sample.timeUs = AMediaExtractor_getSampleTime(extractor);
      samples.push_back(std::move(sample));
      AMediaExtractor_advance(extractor);
    }
  }

  MediaFormat transcode(AMediaExtractor *extractor, AMediaFormat *inputFormat,
                        std::vector<AudioSample> &samples,
                        const std::atomic<bool> &stop) {
    auto mime = mimeOf(inputFormat);
    MediaCodec decoder(AMediaCodec_createDecoderByType(mime.c_str()));
    if (decoder == nullptr) {
      fail("no decoder for " + mime);
    }
    if (AMediaCodec_configure(decoder.get(), inputFormat, nullptr, nullptr,
                              0) != AMEDIA_OK ||
        AMediaCodec_start(decoder.get()) != AMEDIA_OK) {
      fail("cannot decode " + mime);
    }
    int32_t sampleRate = 44100;
    int32_t channels = 2;
    AMediaFormat_getInt32(inputFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                          &sampleRate);
    AMediaFormat_getInt32(inputFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                          &channels);
    // The sound would start late by the samples the codecs add ahead of it,
    // which a player is only told to skip through an edit list MediaMuxer
    // does not write: the source's encoder delay (an MP3's LAME header, which
    // MediaExtractor reports) and the AAC encoder's priming frame. That much
    // decoded sound is dropped, and the AAC is muxed from t = 0. One AAC
    // frame of priming (1024 samples) is what aligns the sound with Android's
    // AAC encoder, c2.android.aac.encoder: measured in the e2e test, where a
    // beep at 1.02 s comes out at 1.02 s. (Media3 sets that encoder's delay to
    // 1600 samples, for a player to skip on decode, which is not this.)
    constexpr int32_t kAacPrimingSamples = 1024;
    int32_t encoderDelay = 0;
    AMediaFormat_getInt32(inputFormat, "encoder-delay", &encoderDelay);
    // An MP3 decoder adds 529 samples of its own (the MDCT and the polyphase
    // filterbank), which the encoder delay does not count.
    constexpr int32_t kMp3DecoderDelaySamples = 529;
    int64_t skipSamples = encoderDelay + kAacPrimingSamples +
                          (mime == "audio/mpeg" ? kMp3DecoderDelaySamples : 0);
    int64_t firstOutputTimeUs = -1;
    MediaCodec encoder;
    MediaFormat outputFormat;
    bool inputDone = false;
    bool decoderDone = false;
    bool encoderDone = false;
    // Decoded PCM waiting for the encoder, with its time.
    std::deque<uint8_t> pcm;
    int64_t pcmTimeUs = 0;
    bool pcmEnded = false;
    auto deadline = std::chrono::steady_clock::now() + kTimeout;
    while (!encoderDone) {
      if (stop.load()) {
        fail("cancelled");
      }
      if (std::chrono::steady_clock::now() > deadline) {
        fail("timed out encoding the sound");
      }
      if (!inputDone) {
        auto index = AMediaCodec_dequeueInputBuffer(decoder.get(), 0);
        if (index >= 0) {
          size_t capacity = 0;
          auto *buffer =
              AMediaCodec_getInputBuffer(decoder.get(), index, &capacity);
          auto size =
              AMediaExtractor_readSampleData(extractor, buffer, capacity);
          auto timeUs = AMediaExtractor_getSampleTime(extractor);
          if (size < 0) {
            AMediaCodec_queueInputBuffer(decoder.get(), index, 0, 0, 0,
                                         AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
            inputDone = true;
          } else {
            AMediaCodec_queueInputBuffer(decoder.get(), index, 0, size, timeUs,
                                         0);
            AMediaExtractor_advance(extractor);
          }
        }
      }
      if (!decoderDone) {
        AMediaCodecBufferInfo info;
        auto index =
            AMediaCodec_dequeueOutputBuffer(decoder.get(), &info, 1000);
        if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
          MediaFormat format(AMediaCodec_getOutputFormat(decoder.get()));
          AMediaFormat_getInt32(format.get(), AMEDIAFORMAT_KEY_SAMPLE_RATE,
                                &sampleRate);
          AMediaFormat_getInt32(format.get(), AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                                &channels);
        } else if (index >= 0) {
          size_t capacity = 0;
          auto *buffer =
              AMediaCodec_getOutputBuffer(decoder.get(), index, &capacity);
          const int64_t frameBytes = 2 * channels;
          const int64_t skipBytes =
              std::min<int64_t>(skipSamples * frameBytes, info.size);
          skipSamples -= skipBytes / frameBytes;
          if (pcm.empty()) {
            pcmTimeUs = info.presentationTimeUs +
                        skipBytes / frameBytes * 1'000'000 / sampleRate;
          }
          pcm.insert(pcm.end(), buffer + info.offset + skipBytes,
                     buffer + info.offset + info.size);
          AMediaCodec_releaseOutputBuffer(decoder.get(), index, false);
          if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            decoderDone = true;
            pcmEnded = true;
          }
          deadline = std::chrono::steady_clock::now() + kTimeout;
        }
      }
      // The encoder is made once the decoder told the PCM's format, which the
      // first output buffer comes after.
      if (encoder == nullptr && (!pcm.empty() || pcmEnded)) {
        encoder.reset(AMediaCodec_createEncoderByType(kAacMime));
        MediaFormat format(AMediaFormat_new());
        AMediaFormat_setString(format.get(), AMEDIAFORMAT_KEY_MIME, kAacMime);
        AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_SAMPLE_RATE,
                              sampleRate);
        AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                              channels);
        AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_BIT_RATE,
                              64000 * channels);
        // AACObjectLC
        AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_AAC_PROFILE, 2);
        AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_MAX_INPUT_SIZE,
                              1 << 16);
        if (encoder == nullptr ||
            AMediaCodec_configure(encoder.get(), format.get(), nullptr, nullptr,
                                  AMEDIACODEC_CONFIGURE_FLAG_ENCODE) !=
                AMEDIA_OK ||
            AMediaCodec_start(encoder.get()) != AMEDIA_OK) {
          fail("cannot encode the sound to AAC");
        }
      }
      if (encoder == nullptr) {
        continue;
      }
      // PCM into the encoder, as much as its buffers take.
      while (!pcm.empty() || pcmEnded) {
        auto index = AMediaCodec_dequeueInputBuffer(encoder.get(), 0);
        if (index < 0) {
          break;
        }
        size_t capacity = 0;
        auto *buffer =
            AMediaCodec_getInputBuffer(encoder.get(), index, &capacity);
        auto size = std::min(capacity, pcm.size());
        std::copy(pcm.begin(), pcm.begin() + size, buffer);
        pcm.erase(pcm.begin(), pcm.begin() + size);
        bool last = pcmEnded && pcm.empty();
        AMediaCodec_queueInputBuffer(
            encoder.get(), index, 0, size, pcmTimeUs,
            last ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0);
        pcmTimeUs += static_cast<int64_t>(size) * 1'000'000 /
                     (2 * channels * sampleRate);
        if (last) {
          pcmEnded = false;
          break;
        }
      }
      AMediaCodecBufferInfo info;
      // Once the decoder is done, only the encoder has work left: wait for it
      // rather than spin.
      auto index = AMediaCodec_dequeueOutputBuffer(encoder.get(), &info,
                                                   decoderDone ? 5000 : 0);
      if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        outputFormat.reset(AMediaCodec_getOutputFormat(encoder.get()));
      } else if (index >= 0) {
        size_t capacity = 0;
        auto *buffer =
            AMediaCodec_getOutputBuffer(encoder.get(), index, &capacity);
        if (info.size > 0 &&
            !(info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG)) {
          if (firstOutputTimeUs < 0) {
            firstOutputTimeUs = info.presentationTimeUs;
          }
          AudioSample sample;
          sample.data.assign(buffer + info.offset,
                             buffer + info.offset + info.size);
          sample.timeUs = info.presentationTimeUs - firstOutputTimeUs;
          samples.push_back(std::move(sample));
        }
        AMediaCodec_releaseOutputBuffer(encoder.get(), index, false);
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
          encoderDone = true;
        }
      }
    }
    if (outputFormat == nullptr) {
      fail("the AAC encoder gave no format");
    }
    return outputFormat;
  }

  std::string _path;
};

// What the drain thread, the finishing thread and the JS thread share. Held
// by every thread the writer starts, so that it outlives a writer the JS
// wrapper dropped.
struct State {
  VideoWriterOptions options;
  MediaCodec codec;
  ANativeWindow *window = nullptr;
  int fd = -1;
  // Whether the file at the path is this writer's to delete, and which file
  // it is: a file another writer made at the same path since is not deleted.
  bool wroteFile = false;
  dev_t fileDevice = 0;
  ino_t fileInode = 0;
  // Set once the end of the stream is signaled: the encoder has to finish.
  std::atomic<bool> endSignaled{false};
  // cancel() calls waiting for a running finish() to be done with the file.
  bool settled = false;
  std::vector<std::function<void()>> onSettled;
  AMediaMuxer *muxer = nullptr;
  bool muxerStarted = false;
  ssize_t videoTrack = -1;
  ssize_t audioTrack = -1;
  // The end of the video, once finishing: the sound stops there.
  std::atomic<int64_t> endUs{INT64_MAX};

  MediaFormat audioFormat;
  std::vector<AudioSample> audioSamples;
  size_t nextAudioSample = 0;
  // Added to the sound's timestamps: see the audio track in drain().
  int64_t audioOffsetUs = 0;
  // Half an AAC frame (1024 samples) of the sound: see writeAudio().
  int64_t audioHalfFrameUs = 0;
  std::thread audioThread;
  std::optional<VideoWriterError> audioError;

  std::thread drainer;
  std::atomic<bool> stop{false};

  std::mutex mutex;
  // The presentation times of the frames appended and not yet out of the
  // encoder, in order (no B-frames: the frames come out as they went in).
  std::deque<int64_t> timestamps;
  std::optional<VideoWriterError> error;

  ~State() {
    stop.store(true);
    if (audioThread.joinable()) {
      audioThread.join();
    }
    if (drainer.joinable()) {
      drainer.join();
    }
    closeFile();
    releaseCodec();
  }

  // Lets go of the encoder and its input window, once nothing draws into the
  // window and the drain thread is done: an encoder is a hardware resource,
  // which the JS object holding this state would otherwise keep until it is
  // garbage collected.
  void releaseCodec() {
    codec = nullptr;
    if (window != nullptr) {
      ANativeWindow_release(window);
      window = nullptr;
    }
  }

  // Calls the cancel() callbacks waiting for the file to be settled.
  void settle() {
    std::vector<std::function<void()>> callbacks;
    {
      std::lock_guard<std::mutex> lock(mutex);
      settled = true;
      callbacks.swap(onSettled);
    }
    for (auto &callback : callbacks) {
      callback();
    }
  }

  // Stops the muxer (writing out the file, if it started) and closes the
  // file. False if either failed.
  bool closeFile() {
    bool ok = true;
    if (muxer != nullptr) {
      if (muxerStarted) {
        ok = AMediaMuxer_stop(muxer) == AMEDIA_OK;
        muxerStarted = false;
      }
      AMediaMuxer_delete(muxer);
      muxer = nullptr;
    }
    if (fd >= 0) {
      ok = close(fd) == 0 && ok;
      fd = -1;
    }
    return ok;
  }

  void fail(const std::string &code, const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!error.has_value()) {
      error = VideoWriterError(code, message);
    }
  }

  std::optional<VideoWriterError> failure() {
    std::lock_guard<std::mutex> lock(mutex);
    return error;
  }

  // Muxes the sound up to `untilUs` (exclusive).
  void writeAudio(int64_t untilUs) {
    // At the end of the video, a frame of sound is kept if most of it plays
    // before the end: MediaMuxer writes no edit list that would cut a frame
    // where the video ends, so the sound ends within half a frame of it.
    if (untilUs >= endUs.load()) {
      untilUs = endUs.load() - audioHalfFrameUs;
    }
    while (audioTrack >= 0 && nextAudioSample < audioSamples.size() &&
           audioSamples[nextAudioSample].timeUs + audioOffsetUs < untilUs) {
      auto &sample = audioSamples[nextAudioSample++];
      AMediaCodecBufferInfo info;
      info.offset = 0;
      info.size = static_cast<int32_t>(sample.data.size());
      info.presentationTimeUs = sample.timeUs + audioOffsetUs;
      info.flags = kKeyFrameFlag;
      AMediaMuxer_writeSampleData(muxer, audioTrack, sample.data.data(), &info);
      std::vector<uint8_t>().swap(sample.data);
    }
  }

  // Takes the encoded frames out of the encoder and muxes them, on a thread
  // of its own: the encoder stops taking frames while its output is full.
  void drain() {
    pthread_setname_np(pthread_self(), "WebGPU VideoWriter");
    auto lastOutput = std::chrono::steady_clock::now();
    int64_t lastInputTimeUs = INT64_MIN;
    for (;;) {
      if (stop.load(std::memory_order_acquire)) {
        return;
      }
      AMediaCodecBufferInfo info{};
      auto index = AMediaCodec_dequeueOutputBuffer(codec.get(), &info,
                                                   kDequeueTimeoutUs);
      if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
        bool waiting = false;
        {
          std::lock_guard<std::mutex> lock(mutex);
          waiting = !timestamps.empty() || endSignaled.load();
        }
        // Only frames that went in and never came out count against it, and
        // the end of the stream once it is signaled.
        if (!waiting) {
          lastOutput = std::chrono::steady_clock::now();
        } else if (std::chrono::steady_clock::now() - lastOutput > kTimeout) {
          fail("encoder-failed", "VideoWriter: timed out waiting for the "
                                 "encoder");
          return;
        }
        continue;
      }
      if (index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
        continue;
      }
      // Anything else that is not a buffer is the codec failing (it was
      // reclaimed, or the media server died). The resource errors are
      // positive.
      // The system takes a codec back from an app in the background when
      // another needs it (MediaCodec.CodecException.ERROR_RECLAIMED).
      if (index == AMEDIACODEC_ERROR_RECLAIMED) {
        fail("interrupted", "VideoWriter: the system reclaimed the encoder");
        return;
      }
      if ((index < 0 && index != AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) ||
          index == AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE) {
        fail("encoder-failed",
             "VideoWriter: the encoder failed (" + std::to_string(index) + ")");
        return;
      }
      lastOutput = std::chrono::steady_clock::now();
      if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        MediaFormat format(AMediaCodec_getOutputFormat(codec.get()));
        videoTrack = AMediaMuxer_addTrack(muxer, format.get());
        // The sound's format is known once it has been read.
        if (audioThread.joinable()) {
          audioThread.join();
        }
        if (audioError.has_value()) {
          fail(audioError->code(), audioError->what());
          return;
        }
        if (audioFormat != nullptr) {
          audioTrack = AMediaMuxer_addTrack(muxer, audioFormat.get());
          int32_t sampleRate = 0;
          if (AMediaFormat_getInt32(audioFormat.get(),
                                    AMEDIAFORMAT_KEY_SAMPLE_RATE,
                                    &sampleRate) &&
              sampleRate > 0) {
            audioHalfFrameUs = 512 * 1'000'000LL / sampleRate;
          }
          // A copied AAC track can start before 0 (its priming, which the
          // source's edit list hid), which MediaMuxer rejects before API
          // level 30: the sound is shifted to start at 0, as Media3 does
          // (FrameworkMuxer.writeSampleData,
          // https://github.com/androidx/media/blob/main/libraries/transformer/src/main/java/androidx/media3/transformer/FrameworkMuxer.java).
          if (android_get_device_api_level() < 30 && !audioSamples.empty() &&
              audioSamples.front().timeUs < 0) {
            audioOffsetUs = -audioSamples.front().timeUs;
          }
        }
        if (videoTrack < 0 || (audioFormat != nullptr && audioTrack < 0) ||
            AMediaMuxer_start(muxer) != AMEDIA_OK) {
          fail("write-failed",
               "VideoWriter: cannot write the tracks of " + options.path);
          return;
        }
        muxerStarted = true;
        continue;
      }
      size_t capacity = 0;
      auto *buffer = AMediaCodec_getOutputBuffer(codec.get(), index, &capacity);
      if (buffer == nullptr) {
        fail("encoder-failed", "VideoWriter: no output buffer from the "
                               "encoder");
        return;
      }
      const bool end = info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
      auto failWith = [&](const char *code, const std::string &message) {
        AMediaCodec_releaseOutputBuffer(codec.get(), index, false);
        fail(code, message);
      };
      if (info.size > 0 &&
          !(info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG)) {
        if (!muxerStarted) {
          failWith("encoder-failed",
                   "VideoWriter: the encoder wrote a frame before its format");
          return;
        }
        // The frames come out in the order they went in (no B-frames), each
        // timed by the window; written at the timestamp it was appended with.
        if (info.presentationTimeUs < lastInputTimeUs) {
          failWith("encoder-failed",
                   "VideoWriter: the encoder reordered the frames");
          return;
        }
        lastInputTimeUs = info.presentationTimeUs;
        int64_t timeUs = 0;
        {
          std::lock_guard<std::mutex> lock(mutex);
          if (timestamps.empty()) {
            if (!error.has_value()) {
              error = VideoWriterError(
                  "encoder-failed",
                  "VideoWriter: the encoder wrote more frames than appended");
            }
            AMediaCodec_releaseOutputBuffer(codec.get(), index, false);
            return;
          }
          timeUs = timestamps.front();
          timestamps.pop_front();
        }
        // The sound up to this frame, so that the file interleaves them.
        writeAudio(std::min(timeUs, endUs.load()));
        AMediaCodecBufferInfo sample = info;
        sample.presentationTimeUs = timeUs;
        sample.flags &= kKeyFrameFlag;
        if (AMediaMuxer_writeSampleData(muxer, videoTrack, buffer, &sample) !=
            AMEDIA_OK) {
          failWith("write-failed",
                   "VideoWriter: writing " + options.path + " failed");
          return;
        }
      }
      AMediaCodec_releaseOutputBuffer(codec.get(), index, false);
      if (end) {
        return;
      }
    }
  }
};

class AndroidVideoWriter : public IVideoWriter {
public:
  explicit AndroidVideoWriter(const VideoWriterOptions &options)
      : _state(std::make_shared<State>()) {
    _state->options = options;
    try {
      start();
    } catch (...) {
      cleanUp();
      throw;
    }
  }

  ~AndroidVideoWriter() override {
    _lender->writer = nullptr;
    if (!_done) {
      cancel(nullptr);
    }
  }

  void dequeueBuffer(std::function<void(VideoFrameHandle)> onBuffer,
                     VideoWriterErrorCallback onError) override {
    if (auto failure = _state->failure()) {
      onError(failure->code(), failure->what());
      return;
    }
    if (_done) {
      onError("invalid-argument",
              "VideoWriter.dequeueBuffer(): the writer is finishing");
      return;
    }
    // The buffers are free again as soon as they are appended: one is free
    // unless the caller holds all of them.
    _pending.push_back({std::move(onBuffer), std::move(onError)});
    servePending();
  }

  void append(void *handle, double timestamp, wgpu::SharedFence fence,
              uint64_t signaledValue) override {
    if (auto failure = _state->failure()) {
      throw *failure;
    }
    if (_done) {
      throw VideoWriterError("invalid-argument",
                             "VideoWriter.append(): the writer is finishing");
    }
    auto *slot = slotOf(handle);
    if (slot == nullptr || !slot->out) {
      throw VideoWriterError(
          "invalid-argument",
          "VideoWriter.append(): the buffer was not dequeued from this "
          "writer, or was appended already");
    }
    if (!std::isfinite(timestamp) || timestamp < 0 ||
        timestamp <= _lastTimestamp) {
      throw VideoWriterError(
          "invalid-argument",
          "VideoWriter.append(): the timestamp must be finite, positive, and "
          "greater than the previous frame's");
    }
    const auto &device = _state->options.device;
    // The buffer is copied into the encoder's input surface once the GPU
    // work that drew it is done: the copy waits for its fences on the GPU.
    // Imported once per buffer, with the texture the copy reads.
    if (slot->memory == nullptr) {
      VideoFrameHandle frame;
      frame.handle = slot->buffer;
      frame.width = _state->options.width;
      frame.height = _state->options.height;
      slot->memory = native_frame::importMemory(device, frame, "VideoWriter",
                                                "VideoWriter.append");
      wgpu::TextureDescriptor textureDesc;
      textureDesc.label = "VideoWriter frame";
      textureDesc.size = {_state->options.width, _state->options.height, 1};
      textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
      textureDesc.usage = wgpu::TextureUsage::CopySrc;
      slot->texture = slot->memory.CreateTexture(&textureDesc);
    }
    const auto &texture = slot->texture;
    std::vector<wgpu::SharedFence> fences;
    std::vector<uint64_t> values;
    if (fence) {
      fences.push_back(std::move(fence));
      values.push_back(signaledValue);
    }
    native_frame::beginAccess(slot->memory, texture, true, fences, values,
                              "VideoWriter.append");
    wgpu::SurfaceTexture surfaceTexture;
    _surface.GetCurrentTexture(&surfaceTexture);
    if (surfaceTexture.texture == nullptr ||
        (surfaceTexture.status !=
             wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
         surfaceTexture.status !=
             wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)) {
      native_frame::endAccess(slot->memory, texture);
      throw VideoWriterError(
          "encoder-failed",
          "VideoWriter: no input buffer from the encoder (status " +
              std::to_string(static_cast<int>(surfaceTexture.status)) + ")");
    }
    auto encoder = device.CreateCommandEncoder();
    wgpu::TexelCopyTextureInfo source;
    source.texture = texture;
    wgpu::TexelCopyTextureInfo destination;
    destination.texture = surfaceTexture.texture;
    wgpu::Extent3D size = {_state->options.width, _state->options.height, 1};
    encoder.CopyTextureToTexture(&source, &destination, &size);
    auto commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
    native_frame::endAccess(slot->memory, texture);
    {
      std::lock_guard<std::mutex> lock(_state->mutex);
      _state->timestamps.push_back(std::llround(timestamp * 1'000'000.0));
    }
    // Queued with the fence of the copy: the encoder waits for the GPU.
    _surface.Present();
    _lastTimestamp = timestamp;
    _frames++;
    // The copy is ordered on the queue before any later drawing into the
    // buffer: it is free again.
    slot->out = false;
    servePending();
  }

  void finish(std::function<void()> onDone,
              VideoWriterErrorCallback onError) override {
    if (_done) {
      auto failure = _state->failure();
      onError(failure ? failure->code() : "invalid-argument",
              failure ? failure->what()
                      : "VideoWriter.finish(): the writer is finishing "
                        "already");
      return;
    }
    _done = true;
    failPending("invalid-argument", "VideoWriter: the writer finished");
    if (auto failure = _state->failure()) {
      cleanUp();
      onError(failure->code(), failure->what());
      return;
    }
    if (_frames == 0) {
      cleanUp();
      onError("invalid-argument",
              "VideoWriter.finish(): no frame was appended");
      return;
    }
    // Dawn's swapchain lets go of the encoder's window first, on this thread.
    releaseSurface();
    auto state = _state;
    state->endUs = std::llround(
        (_lastTimestamp + 1.0 / state->options.frameRate) * 1'000'000.0);
    std::thread([state, onDone, onError]() {
      if (AMediaCodec_signalEndOfInputStream(state->codec.get()) != AMEDIA_OK) {
        state->fail("encoder-failed",
                    "VideoWriter: the encoder did not take the end of the "
                    "stream");
        state->stop.store(true);
      }
      state->endSignaled.store(true);
      if (state->drainer.joinable()) {
        state->drainer.join();
      }
      auto failure = state->failure();
      // Every frame appended has to come out of the encoder: one that did not
      // fails the export rather than shortening the video.
      if (!failure.has_value()) {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->timestamps.empty()) {
          failure = VideoWriterError(
              "encoder-failed", "VideoWriter: the encoder ended with " +
                                    std::to_string(state->timestamps.size()) +
                                    " frame(s) appended but not encoded");
        }
      }
      if (!failure.has_value() && !state->muxerStarted) {
        failure = VideoWriterError("encoder-failed", "VideoWriter: the encoder "
                                                     "wrote nothing");
      }
      if (!failure.has_value()) {
        // The sound left until the video ends; the rest is cut.
        state->writeAudio(state->endUs.load());
        // MediaMuxer gives the last frame the duration of the one before,
        // which frames far apart make long: an empty end-of-stream sample
        // ends the track 1 / frameRate after the last frame, as Media3 does
        // (FrameworkMuxer.close,
        // https://github.com/androidx/media/blob/main/libraries/transformer/src/main/java/androidx/media3/transformer/FrameworkMuxer.java).
        AMediaCodecBufferInfo end{};
        end.presentationTimeUs = state->endUs.load();
        end.flags = AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
        const uint8_t none = 0;
        AMediaMuxer_writeSampleData(state->muxer, state->videoTrack, &none,
                                    &end);
        if (!state->closeFile()) {
          failure = VideoWriterError("write-failed", "VideoWriter: finishing " +
                                                         state->options.path +
                                                         " failed");
        }
      }
      if (failure.has_value()) {
        stopAndDelete(*state);
      } else {
        // Written out: the file is the app's now.
        state->wroteFile = false;
      }
      state->releaseCodec();
      state->settle();
      if (failure.has_value()) {
        onError(failure->code(), failure->what());
      } else {
        onDone();
      }
    }).detach();
  }

  void cancel(std::function<void()> onDone) noexcept override {
    _state->fail("cancelled", "VideoWriter: cancelled");
    failPending("cancelled", "VideoWriter: cancelled");
    if (!_done) {
      _done = true;
      cleanUp();
    }
    if (!onDone) {
      return;
    }
    // A finish() running on its thread fails on its own: done once it is.
    {
      std::lock_guard<std::mutex> lock(_state->mutex);
      if (!_state->settled) {
        _state->onSettled.push_back(std::move(onDone));
        return;
      }
    }
    onDone();
  }

private:
  struct Slot {
    AHardwareBuffer *buffer = nullptr;
    bool out = false;
    // Counts the times the buffer was handed out: a release that is not of
    // the current loan is ignored.
    uint64_t loan = 0;
    wgpu::SharedTextureMemory memory;
    wgpu::Texture texture;
  };
  struct Pending {
    std::function<void(VideoFrameHandle)> onBuffer;
    VideoWriterErrorCallback onError;
  };

  // The format of the video encoder, from the most to the least demanding:
  // each fallback drops a setting some encoders refuse at configure time.
  MediaFormat videoFormat(int attempt) const {
    const auto &options = _state->options;
    MediaFormat format(AMediaFormat_new());
    AMediaFormat_setString(format.get(), AMEDIAFORMAT_KEY_MIME, kVideoMime);
    AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_WIDTH,
                          static_cast<int32_t>(options.width));
    AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_HEIGHT,
                          static_cast<int32_t>(options.height));
    AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_COLOR_FORMAT,
                          kColorFormatSurface);
    AMediaFormat_setInt32(
        format.get(), AMEDIAFORMAT_KEY_BIT_RATE,
        static_cast<int32_t>(std::min<int64_t>(options.bitrate, INT32_MAX)));
    // KEY_FRAME_RATE only budgets the bitrate (the timestamps time the
    // frames), and some encoders refuse a high one at configure time even
    // when their capabilities list it: the last fallback asks for 30. Media3
    // falls back to its default rate on one such device
    // (DefaultEncoderFactory.deviceNeedsDefaultFrameRateWorkaround), and
    // react-native-skia-video steps down 60, 45, 30 until configure succeeds
    // (https://github.com/AzzappApp/react-native-skia-video/blob/main/android/src/main/java/com/azzapp/rnskv/VideoCapabilities.java).
    const double frameRate =
        attempt >= 2 ? std::min(options.frameRate, 30.0) : options.frameRate;
    if (frameRate == std::floor(frameRate)) {
      AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_FRAME_RATE,
                            static_cast<int32_t>(frameRate));
    } else {
      AMediaFormat_setFloat(format.get(), AMEDIAFORMAT_KEY_FRAME_RATE,
                            static_cast<float>(frameRate));
    }
    AMediaFormat_setInt32(format.get(), AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 2);
    // The frames reach the encoder as fast as they are drawn, and the window
    // stamps each with the time it was queued: no frame rate the encoder
    // could budget its bitrate with. A negative gap makes it stamp the frames
    // 1 / frameRate apart instead (MediaFormat KEY_MAX_PTS_GAP_TO_ENCODER,
    // API level 29); the muxer then writes them at their own timestamps.
    AMediaFormat_setInt64(format.get(), "max-pts-gap-to-encoder",
                          -std::llround(1'000'000.0 / options.frameRate));
    // No B-frames (KEY_MAX_B_FRAMES): the frames come out in the order they
    // went in, which the drain relies on to time them.
    AMediaFormat_setInt32(format.get(), "max-bframes", 0);
    // Not real time (KEY_PRIORITY 1, best effort) and as fast as the encoder
    // goes (KEY_OPERATING_RATE), as Media3 configures its encoders
    // (DefaultEncoderFactory.adjustMediaFormatForEncoderPerformanceSettings):
    // without an operating rate some encoders pace an offline export in real
    // time. Integer.MAX_VALUE throws at configure time on some chipsets
    // (SM8550, SM7450, SM6450, SC9863A, T612, T606 and T603 on API 31 to 34,
    // Media3's deviceNeedsLowerOperatingRateAvoidingOverflowWorkaround), which
    // take 1000; rather than a list of chipsets, the first fallback asks for
    // 1000 and the second for none.
    AMediaFormat_setInt32(format.get(), "priority", 1);
    if (attempt == 0) {
      AMediaFormat_setInt32(format.get(), "operating-rate", INT32_MAX);
    } else if (attempt == 1) {
      AMediaFormat_setInt32(format.get(), "operating-rate", 1000);
    }
    // The frames are sRGB: BT.709, limited range (KEY_COLOR_STANDARD,
    // KEY_COLOR_TRANSFER, KEY_COLOR_RANGE).
    AMediaFormat_setInt32(format.get(), "color-standard", 1);
    AMediaFormat_setInt32(format.get(), "color-transfer", 3);
    AMediaFormat_setInt32(format.get(), "color-range", 2);
    return format;
  }

  // Configures the video encoder with the first format it takes. A codec a
  // configure failed on is in an error state: each attempt makes a new one.
  void configureEncoder() {
    auto &state = *_state;
    const auto &options = state.options;
    constexpr int kAttempts = 3;
    media_status_t status = AMEDIA_OK;
    for (int attempt = 0; attempt < kAttempts; attempt++) {
      state.codec.reset(AMediaCodec_createEncoderByType(kVideoMime));
      if (state.codec == nullptr) {
        throw VideoWriterError("unsupported",
                               "VideoWriter: this device has no H.264 encoder");
      }
      auto format = videoFormat(attempt);
      status =
          AMediaCodec_configure(state.codec.get(), format.get(), nullptr,
                                nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
      if (status == AMEDIA_OK) {
        return;
      }
    }
    state.codec = nullptr;
    throw VideoWriterError("unsupported",
                           "VideoWriter: the H.264 encoder does not take " +
                               std::to_string(options.width) + "x" +
                               std::to_string(options.height) + " at " +
                               std::to_string(options.bitrate) + " bps (" +
                               std::to_string(status) + ")");
  }

  void start() {
    auto &state = *_state;
    auto &options = state.options;
    // The frames are timed by the order they leave the encoder in, which
    // takes two keys from API level 29: KEY_MAX_B_FRAMES (no B-frames, so
    // the frames leave in the order they went in) and
    // KEY_MAX_PTS_GAP_TO_ENCODER (see configureEncoder). Older encoders ignore
    // both, and some reorder the frames regardless: Media3 has to force the
    // Baseline profile on API 27 devices whose encoder emits B-frames in High
    // (DefaultEncoderFactory.deviceNeedsNoH264HighProfileWorkaround,
    // https://github.com/androidx/media/blob/main/libraries/transformer/src/main/java/androidx/media3/transformer/DefaultEncoderFactory.java).
    if (android_get_device_api_level() < 29) {
      throw VideoWriterError("unsupported",
                             "VideoWriter: Android 10 (API level 29) or "
                             "later is required");
    }
    if (options.audioPath.has_value()) {
      // Read on a thread of its own while the frames are drawn: its format
      // must be known when the muxer starts, as soon as the video's is.
      auto path = *options.audioPath;
      if (access(path.c_str(), R_OK) != 0) {
        throw VideoWriterError("audio-failed",
                               "VideoWriter: no sound file at " + path);
      }
      auto *raw = _state.get();
      state.audioThread = std::thread([raw, path]() {
        try {
          raw->audioFormat =
              AudioReader(path).read(raw->audioSamples, raw->stop);
        } catch (const VideoWriterError &e) {
          raw->audioError = e;
        }
      });
    }

    configureEncoder();
    if (AMediaCodec_createInputSurface(state.codec.get(), &state.window) !=
            AMEDIA_OK ||
        state.window == nullptr) {
      throw VideoWriterError("unsupported",
                             "VideoWriter: the encoder has no input surface");
    }
    if (AMediaCodec_start(state.codec.get()) != AMEDIA_OK) {
      throw VideoWriterError("encoder-failed",
                             "VideoWriter: the encoder did not start");
    }

    // The surface is made on the device's own instance: a device imported
    // from another library (Skia's) belongs to that library's instance.
    auto device = options.device;
    auto instance = device.GetAdapter().GetInstance();
    _surface = WebGPURuntime::makeSurface(instance, state.window);
    wgpu::SurfaceCapabilities capabilities;
    bool rgba = false;
    if (_surface != nullptr &&
        _surface.GetCapabilities(device.GetAdapter(), &capabilities) ==
            wgpu::Status::Success) {
      for (size_t i = 0; i < capabilities.formatCount; i++) {
        rgba |= capabilities.formats[i] == wgpu::TextureFormat::RGBA8Unorm;
      }
    }
    if (!rgba || !(capabilities.usages & wgpu::TextureUsage::CopyDst)) {
      throw VideoWriterError("unsupported",
                             "VideoWriter: the GPU cannot copy into the "
                             "encoder's input surface");
    }
    wgpu::SurfaceConfiguration config;
    config.device = device;
    config.format = wgpu::TextureFormat::RGBA8Unorm;
    config.width = options.width;
    config.height = options.height;
    // Every frame is kept: the encoder's queue holds the next one back until
    // it took the last.
    config.presentMode = wgpu::PresentMode::Fifo;
    config.usage = wgpu::TextureUsage::CopyDst;
    _surface.Configure(&config);
    _configured = true;

    for (size_t i = 0; i < kBufferCount; i++) {
      AHardwareBuffer_Desc desc = {};
      desc.width = options.width;
      desc.height = options.height;
      desc.layers = 1;
      desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
      desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                   AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
      AHardwareBuffer *buffer = nullptr;
      if (AHardwareBuffer_allocate(&desc, &buffer) != 0 || buffer == nullptr) {
        throw VideoWriterError("unsupported",
                               "VideoWriter: cannot allocate the frame "
                               "buffers");
      }
      _buffers.push_back(Slot{buffer});
    }

    state.fd = open(options.path.c_str(),
                    O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC, 0644);
    if (state.fd < 0) {
      throw VideoWriterError("write-failed", "VideoWriter: cannot write " +
                                                 options.path + ": " +
                                                 std::strerror(errno));
    }
    state.wroteFile = true;
    struct stat info;
    if (fstat(state.fd, &info) == 0) {
      state.fileDevice = info.st_dev;
      state.fileInode = info.st_ino;
    }
    state.muxer = AMediaMuxer_new(state.fd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
    if (state.muxer == nullptr) {
      throw VideoWriterError("write-failed",
                             "VideoWriter: cannot write " + options.path);
    }
    auto *raw = _state.get();
    state.drainer = std::thread([raw]() {
      raw->drain();
      // A failed drain stops the encoder: its input window then refuses
      // frames rather than blocking the JS thread presenting one.
      if (raw->failure().has_value()) {
        AMediaCodec_stop(raw->codec.get());
      }
    });
  }

  // Lends a buffer to JS. Released without being appended, it goes back
  // unencoded (on the JS thread, where NativeBuffers are released).
  VideoFrameHandle lend(Slot &slot) {
    slot.out = true;
    const uint64_t loan = ++slot.loan;
    AHardwareBuffer *buffer = slot.buffer;
    AHardwareBuffer_acquire(buffer);
    VideoFrameHandle handle;
    handle.handle = buffer;
    handle.width = _state->options.width;
    handle.height = _state->options.height;
    handle.colorSpace.primaries = wgpu::ColorSpacePrimariesDawn::SRGB;
    std::weak_ptr<Lender> lender = _lender;
    handle.deleter = [lender, buffer, loan]() {
      if (auto alive = lender.lock(); alive && alive->writer != nullptr) {
        alive->writer->returned(buffer, loan);
      }
      AHardwareBuffer_release(buffer);
    };
    return handle;
  }

  // A lent buffer released: free again unless it was appended.
  void returned(AHardwareBuffer *buffer, uint64_t loan) {
    auto *slot = slotOf(buffer);
    if (slot != nullptr && slot->out && slot->loan == loan) {
      slot->out = false;
      servePending();
    }
  }

  Slot *slotOf(void *handle) {
    for (auto &slot : _buffers) {
      if (slot.buffer == handle) {
        return &slot;
      }
    }
    return nullptr;
  }

  void servePending() {
    while (!_pending.empty()) {
      Slot *free = nullptr;
      for (auto &slot : _buffers) {
        if (!slot.out) {
          free = &slot;
          break;
        }
      }
      if (free == nullptr) {
        return;
      }
      auto pending = std::move(_pending.front());
      _pending.pop_front();
      pending.onBuffer(lend(*free));
    }
  }

  void failPending(const std::string &code, const std::string &message) {
    auto pending = std::move(_pending);
    _pending.clear();
    for (auto &entry : pending) {
      entry.onError(code, message);
    }
  }

  void releaseSurface() {
    if (_configured) {
      _surface.Unconfigure();
      _configured = false;
    }
    _surface = nullptr;
    for (auto &slot : _buffers) {
      slot.texture = nullptr;
      slot.memory = nullptr;
      AHardwareBuffer_release(slot.buffer);
    }
    _buffers.clear();
  }

  // Stops the encoder and deletes the partial file. Runs on any thread.
  static void stopAndDelete(State &state) {
    state.stop.store(true, std::memory_order_release);
    if (state.drainer.joinable() &&
        state.drainer.get_id() != std::this_thread::get_id()) {
      state.drainer.join();
    }
    state.closeFile();
    state.releaseCodec();
    // Only the file this writer made, once: a file at the path it never got
    // to open, or one another writer made there since, is left as it was.
    struct stat info;
    if (state.wroteFile && stat(state.options.path.c_str(), &info) == 0 &&
        info.st_dev == state.fileDevice && info.st_ino == state.fileInode) {
      unlink(state.options.path.c_str());
    }
    state.wroteFile = false;
  }

  // Gives up the export on this thread: the file is settled after it.
  void cleanUp() {
    releaseSurface();
    stopAndDelete(*_state);
    _state->settle();
  }

  // What a lent buffer's release reaches the writer through, while it lives.
  struct Lender {
    AndroidVideoWriter *writer;
  };
  std::shared_ptr<Lender> _lender = std::make_shared<Lender>(Lender{this});
  std::shared_ptr<State> _state;
  wgpu::Surface _surface;
  bool _configured = false;
  std::vector<Slot> _buffers;
  std::deque<Pending> _pending;
  double _lastTimestamp = -1;
  int64_t _frames = 0;
  bool _done = false;
};

} // namespace

std::unique_ptr<IVideoWriter>
createAndroidVideoWriter(const VideoWriterOptions &options) {
  return std::make_unique<AndroidVideoWriter>(options);
}

} // namespace rnwgpu
