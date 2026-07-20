#pragma once

#include <android/bitmap.h>
#include <android/hardware_buffer.h>
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fbjni/fbjni.h>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"

namespace rnwgpu {

namespace jni = facebook::jni;

class AndroidWorkerPool final {
public:
  AndroidWorkerPool(std::size_t workerCount, std::size_t maxPendingTasks)
      : _maxPendingTasks(maxPendingTasks) {
    if (workerCount == 0 || maxPendingTasks == 0) {
      throw std::invalid_argument(
          "AndroidWorkerPool requires workers and queue capacity");
    }

    try {
      _workers.reserve(workerCount);
      for (std::size_t index = 0; index < workerCount; ++index) {
        _workers.emplace_back([this]() { workerLoop(); });
      }
    } catch (...) {
      shutdown();
      throw;
    }
  }

  ~AndroidWorkerPool() { shutdown(); }

  AndroidWorkerPool(const AndroidWorkerPool &) = delete;
  AndroidWorkerPool &operator=(const AndroidWorkerPool &) = delete;
  AndroidWorkerPool(AndroidWorkerPool &&) = delete;
  AndroidWorkerPool &operator=(AndroidWorkerPool &&) = delete;

  bool submit(std::function<void()> task, std::function<void()> cancel) {
    if (!task || !cancel) {
      return false;
    }

    {
      std::lock_guard<std::mutex> lock(_mutex);
      if (_stopping || _tasks.size() >= _maxPendingTasks) {
        return false;
      }
      _tasks.push_back(
          PendingTask{.run = std::move(task), .cancel = std::move(cancel)});
    }
    _condition.notify_one();
    return true;
  }

  void shutdown() noexcept {
    std::deque<PendingTask> cancelledTasks;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _stopping = true;
      // Pool shutdown happens only at native-library/process teardown. Session
      // reloads invalidate their ref-counted decode state without stopping the
      // shared workers.
      cancelledTasks.swap(_tasks);
    }
    _condition.notify_all();
    cancelTasks(cancelledTasks);

    for (auto &worker : _workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    _workers.clear();
  }

private:
  struct PendingTask final {
    std::function<void()> run;
    std::function<void()> cancel;
  };

  static void cancelTasks(std::deque<PendingTask> &tasks) noexcept {
    for (auto &task : tasks) {
      if (!task.cancel) {
        continue;
      }
      try {
        task.cancel();
      } catch (...) {
        // Cancellation runs during reload/worker failure and must not escape.
      }
    }
    tasks.clear();
  }

  void workerLoop() noexcept {
    try {
      // Pool threads are native-owned. Keep the JVM attachment scoped to the
      // complete worker lifetime so fbjni detaches before std::thread exits.
      jni::ThreadScope threadScope;
      runWorkerLoop();
    } catch (...) {
      // ThreadScope construction can fail when fbjni/JVM initialization is no
      // longer available. Stop accepting work without allowing an exception
      // to escape the noexcept std::thread entry point.
      stopAfterWorkerFailure();
    }
  }

  void runWorkerLoop() noexcept {
    for (;;) {
      PendingTask task;
      {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock,
                        [this]() { return _stopping || !_tasks.empty(); });
        if (_stopping) {
          return;
        }
        task = std::move(_tasks.front());
        _tasks.pop_front();
      }

      try {
        task.run();
      } catch (...) {
        // Individual tasks report their own failures. Never terminate a
        // long-lived worker because an error callback threw.
      }
    }
  }

  void stopAfterWorkerFailure() noexcept {
    std::deque<PendingTask> cancelledTasks;
    try {
      std::lock_guard<std::mutex> lock(_mutex);
      _stopping = true;
      cancelledTasks.swap(_tasks);
    } catch (...) {
      // There is no recovery path if mutex acquisition itself fails. Preserve
      // the noexcept worker boundary and let pool destruction reclaim tasks.
    }
    _condition.notify_all();
    cancelTasks(cancelledTasks);
  }

  const std::size_t _maxPendingTasks;
  std::mutex _mutex;
  std::condition_variable _condition;
  std::deque<PendingTask> _tasks;
  std::vector<std::thread> _workers;
  bool _stopping{false};
};

class AndroidImageDecodeState final {
public:
  explicit AndroidImageDecodeState(jni::global_ref<jobject> blobModule)
      : _blobModule(std::move(blobModule)) {
    if (!_blobModule) {
      throw std::invalid_argument(
          "AndroidImageDecodeState requires a BlobModule");
    }
  }

  AndroidImageDecodeState(const AndroidImageDecodeState &) = delete;
  AndroidImageDecodeState &operator=(const AndroidImageDecodeState &) = delete;
  AndroidImageDecodeState(AndroidImageDecodeState &&) = delete;
  AndroidImageDecodeState &operator=(AndroidImageDecodeState &&) = delete;

  [[nodiscard]] bool isActive() const noexcept {
    return _active.load(std::memory_order_acquire);
  }

  void invalidate() noexcept {
    _active.store(false, std::memory_order_release);
  }

  [[nodiscard]] jobject blobModule() const noexcept {
    return _blobModule.get();
  }

private:
  jni::global_ref<jobject> _blobModule;
  std::atomic<bool> _active{true};
};

inline AndroidWorkerPool &androidImageWorkerPool() {
  // Shared workers outlive individual React Native runtime generations. This
  // lets module invalidation detach immediately while an already-running
  // BitmapFactory decode safely finishes with its own ref-counted JNI state.
  static AndroidWorkerPool pool{2, 32};
  return pool;
}

class AndroidPlatformContext : public PlatformContext {
private:
  template <typename T> class LocalRef final {
  public:
    LocalRef(JNIEnv *env, T value) noexcept : _env(env), _value(value) {}
    ~LocalRef() {
      if (_value) {
        _env->DeleteLocalRef(_value);
      }
    }

    LocalRef(const LocalRef &) = delete;
    LocalRef &operator=(const LocalRef &) = delete;
    LocalRef(LocalRef &&) = delete;
    LocalRef &operator=(LocalRef &&) = delete;

    [[nodiscard]] T get() const noexcept { return _value; }
    [[nodiscard]] explicit operator bool() const noexcept {
      return _value != nullptr;
    }

  private:
    JNIEnv *_env;
    T _value;
  };

  class LockedBitmapPixels final {
  public:
    LockedBitmapPixels(JNIEnv *env, jobject bitmap)
        : _env(env), _bitmap(bitmap) {
      const int status = AndroidBitmap_lockPixels(_env, _bitmap, &_pixels);
      _locked = status == ANDROID_BITMAP_RESULT_SUCCESS && _pixels != nullptr;
      if (_env->ExceptionCheck() == JNI_TRUE) {
        _env->ExceptionClear();
        if (_locked) {
          AndroidBitmap_unlockPixels(_env, _bitmap);
          _locked = false;
          if (_env->ExceptionCheck() == JNI_TRUE) {
            _env->ExceptionClear();
          }
        }
        throw std::runtime_error(
            "AndroidBitmap_lockPixels failed with a Java exception");
      }
      if (!_locked) {
        throw std::runtime_error("Couldn't lock bitmap pixels");
      }
    }

    ~LockedBitmapPixels() {
      if (_locked) {
        AndroidBitmap_unlockPixels(_env, _bitmap);
        if (_env->ExceptionCheck() == JNI_TRUE) {
          _env->ExceptionClear();
        }
      }
    }

    LockedBitmapPixels(const LockedBitmapPixels &) = delete;
    LockedBitmapPixels &operator=(const LockedBitmapPixels &) = delete;
    LockedBitmapPixels(LockedBitmapPixels &&) = delete;
    LockedBitmapPixels &operator=(LockedBitmapPixels &&) = delete;

    [[nodiscard]] const uint8_t *data() const noexcept {
      return static_cast<const uint8_t *>(_pixels);
    }

    void unlock() {
      if (!_locked) {
        return;
      }
      _locked = false;
      const int status = AndroidBitmap_unlockPixels(_env, _bitmap);
      if (_env->ExceptionCheck() == JNI_TRUE) {
        _env->ExceptionClear();
        throw std::runtime_error(
            "AndroidBitmap_unlockPixels failed with a Java exception");
      }
      if (status != ANDROID_BITMAP_RESULT_SUCCESS) {
        throw std::runtime_error("Couldn't unlock bitmap pixels");
      }
    }

  private:
    JNIEnv *_env;
    jobject _bitmap;
    void *_pixels{nullptr};
    bool _locked{false};
  };

  static void throwIfJavaException(JNIEnv *env, const char *operation) {
    if (env->ExceptionCheck() != JNI_TRUE) {
      return;
    }
    env->ExceptionClear();
    throw std::runtime_error(std::string(operation) +
                             " failed with a Java exception");
  }

  static jint checkedBlobRangeValue(double value, const char *name) {
    constexpr double maxJint =
        static_cast<double>(std::numeric_limits<jint>::max());
    if (!std::isfinite(value) || value < 0.0 || std::trunc(value) != value ||
        value > maxJint) {
      throw std::invalid_argument(std::string("Blob ") + name +
                                  " must be a non-negative 32-bit integer");
    }
    return static_cast<jint>(value);
  }

  static std::size_t checkedMultiply(std::size_t left, std::size_t right,
                                     const char *message) {
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
      throw std::overflow_error(message);
    }
    return left * right;
  }

  static constexpr const char *kDecodeCancelledError =
      "Android image decode was cancelled during session teardown";

  std::shared_ptr<AndroidImageDecodeState> _decodeState;

  static std::vector<uint8_t>
  resolveBlob(const std::shared_ptr<AndroidImageDecodeState> &decodeState,
              JNIEnv *env, const std::string &blobId, double offset,
              double size) {
    const auto blobModule = decodeState ? decodeState->blobModule() : nullptr;
    if (blobModule == nullptr) {
      throw std::runtime_error("BlobModule instance is null");
    }

    const jint checkedOffset = checkedBlobRangeValue(offset, "offset");
    const jint checkedSize = checkedBlobRangeValue(size, "size");
    if (checkedOffset > std::numeric_limits<jint>::max() - checkedSize) {
      throw std::invalid_argument("Blob offset plus size exceeds 32-bit range");
    }

    LocalRef<jclass> blobModuleClass(env, env->GetObjectClass(blobModule));
    throwIfJavaException(env, "GetObjectClass(BlobModule)");
    if (!blobModuleClass) {
      throw std::runtime_error("Couldn't find BlobModule class");
    }

    jmethodID resolveMethod = env->GetMethodID(blobModuleClass.get(), "resolve",
                                               "(Ljava/lang/String;II)[B");
    throwIfJavaException(env, "GetMethodID(BlobModule.resolve)");
    if (!resolveMethod) {
      throw std::runtime_error("Couldn't find resolve method in BlobModule");
    }

    LocalRef<jstring> jBlobId(env, env->NewStringUTF(blobId.c_str()));
    throwIfJavaException(env, "NewStringUTF(blobId)");
    if (!jBlobId) {
      throw std::runtime_error("Couldn't allocate Blob identifier");
    }

    LocalRef<jbyteArray> blobData(
        env, static_cast<jbyteArray>(env->CallObjectMethod(
                 blobModule, resolveMethod, jBlobId.get(), checkedOffset,
                 checkedSize)));
    throwIfJavaException(env, "BlobModule.resolve");
    if (!blobData) {
      throw std::runtime_error("Couldn't retrieve blob data");
    }

    const jsize len = env->GetArrayLength(blobData.get());
    throwIfJavaException(env, "GetArrayLength(Blob data)");
    std::vector<uint8_t> data(static_cast<std::size_t>(len));
    if (len > 0) {
      env->GetByteArrayRegion(blobData.get(), 0, len,
                              reinterpret_cast<jbyte *>(data.data()));
      throwIfJavaException(env, "GetByteArrayRegion(Blob data)");
    }
    return data;
  }

public:
  explicit AndroidPlatformContext(jni::global_ref<jobject> blobModule)
      : _decodeState(std::make_shared<AndroidImageDecodeState>(
            std::move(blobModule))) {}
  ~AndroidPlatformContext() override {
    // Running workers retain AndroidImageDecodeState, including BlobModule.
    // Mark their results stale without waiting for BitmapFactory during reload.
    if (_decodeState) {
      _decodeState->invalidate();
    }
  }

  wgpu::Surface makeSurface(wgpu::Instance instance, void *window, int width,
                            int height) override {
    wgpu::SurfaceSourceAndroidNativeWindow androidSurfaceDesc;
    androidSurfaceDesc.window = reinterpret_cast<ANativeWindow *>(window);
    wgpu::SurfaceDescriptor surfaceDescriptor;
    surfaceDescriptor.nextInChain = &androidSurfaceDesc;
    return instance.CreateSurface(&surfaceDescriptor);
  }

  ImageData createImageBitmap(std::string blobId, double offset,
                              double size) override {
    const auto decodeState = _decodeState;
    if (!decodeState || !decodeState->isActive()) {
      throw std::runtime_error(kDecodeCancelledError);
    }
    jni::Environment::ensureCurrentThreadIsAttached();

    JNIEnv *env = facebook::jni::Environment::current();
    if (!env) {
      throw std::runtime_error("Couldn't get JNI environment");
    }

    auto data = resolveBlob(decodeState, env, blobId, offset, size);
    return decodeImageBitmapFromData(data);
  }

  void
  createImageBitmapAsync(std::string blobId, double offset, double size,
                         std::function<void(ImageData)> onSuccess,
                         std::function<void(std::string)> onError) override {
    const auto decodeState = _decodeState;
    auto task = [decodeState, blobId = std::move(blobId), offset, size,
                 onSuccess = std::move(onSuccess), onError]() mutable {
      if (!decodeState || !decodeState->isActive()) {
        onError(kDecodeCancelledError);
        return;
      }
      try {
        JNIEnv *env = facebook::jni::Environment::current();
        if (!env) {
          throw std::runtime_error("Couldn't get JNI environment");
        }
        auto data = resolveBlob(decodeState, env, blobId, offset, size);
        if (!decodeState->isActive()) {
          onError(kDecodeCancelledError);
          return;
        }
        auto result = decodeImageBitmapFromData(data);
        if (!decodeState->isActive()) {
          onError(kDecodeCancelledError);
          return;
        }
        onSuccess(std::move(result));
      } catch (const std::exception &error) {
        onError(error.what());
      } catch (...) {
        onError("Unknown error while decoding a Blob image");
      }
    };

    auto cancel = [onError]() {
      onError("Android image worker pool is shutting down");
    };
    if (!androidImageWorkerPool().submit(std::move(task), std::move(cancel))) {
      onError("Android image worker queue is full or shutting down");
    }
  }

  static ImageData
  decodeImageBitmapFromData(std::span<const uint8_t> data) {
    jni::Environment::ensureCurrentThreadIsAttached();

    JNIEnv *env = facebook::jni::Environment::current();
    if (!env) {
      throw std::runtime_error("Couldn't get JNI environment");
    }

    if (data.size() >
        static_cast<std::size_t>(std::numeric_limits<jsize>::max())) {
      throw std::invalid_argument("Encoded image is too large for JNI");
    }
    const auto encodedSize = static_cast<jsize>(data.size());

    LocalRef<jbyteArray> byteArray(env, env->NewByteArray(encodedSize));
    throwIfJavaException(env, "NewByteArray(encoded image)");
    if (!byteArray) {
      throw std::runtime_error("Couldn't allocate byte array");
    }
    if (encodedSize > 0) {
      env->SetByteArrayRegion(byteArray.get(), 0, encodedSize,
                              reinterpret_cast<const jbyte *>(data.data()));
      throwIfJavaException(env, "SetByteArrayRegion(encoded image)");
    }

    LocalRef<jclass> bitmapFactoryClass(
        env, env->FindClass("android/graphics/BitmapFactory"));
    throwIfJavaException(env, "FindClass(BitmapFactory)");
    if (!bitmapFactoryClass) {
      throw std::runtime_error("Couldn't find BitmapFactory class");
    }
    jmethodID decodeByteArrayMethod =
        env->GetStaticMethodID(bitmapFactoryClass.get(), "decodeByteArray",
                               "([BII)Landroid/graphics/Bitmap;");
    throwIfJavaException(env,
                         "GetStaticMethodID(BitmapFactory.decodeByteArray)");
    if (!decodeByteArrayMethod) {
      throw std::runtime_error("Couldn't find decodeByteArray method");
    }

    LocalRef<jobject> bitmap(
        env, env->CallStaticObjectMethod(bitmapFactoryClass.get(),
                                         decodeByteArrayMethod, byteArray.get(),
                                         0, encodedSize));
    throwIfJavaException(env, "BitmapFactory.decodeByteArray");
    if (!bitmap) {
      throw std::runtime_error("Couldn't decode image");
    }

    AndroidBitmapInfo bitmapInfo{};
    const int bitmapInfoStatus =
        AndroidBitmap_getInfo(env, bitmap.get(), &bitmapInfo);
    throwIfJavaException(env, "AndroidBitmap_getInfo");
    if (bitmapInfoStatus != ANDROID_BITMAP_RESULT_SUCCESS) {
      throw std::runtime_error("Couldn't get bitmap info");
    }

    if (bitmapInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
      throw std::runtime_error("Decoded bitmap is not RGBA_8888");
    }
    if (bitmapInfo.width == 0 || bitmapInfo.height == 0) {
      throw std::runtime_error("Decoded bitmap has empty dimensions");
    }

    const std::size_t width = static_cast<std::size_t>(bitmapInfo.width);
    const std::size_t height = static_cast<std::size_t>(bitmapInfo.height);
    const std::size_t bytesPerRow =
        checkedMultiply(width, 4, "Decoded bitmap row size overflow");
    if (bitmapInfo.stride < bytesPerRow) {
      throw std::runtime_error("Decoded bitmap stride is smaller than a row");
    }
    const std::size_t outputSize = checkedMultiply(
        height, bytesPerRow, "Decoded bitmap buffer size overflow");

    LockedBitmapPixels bitmapPixels(env, bitmap.get());
    ImageData result{};
    result.width = width;
    result.height = height;
    result.format = wgpu::TextureFormat::RGBA8Unorm;
    if (outputSize > result.data.max_size()) {
      throw std::length_error("Decoded bitmap exceeds native buffer capacity");
    }
    result.data.resize(outputSize);
    for (std::size_t row = 0; row < height; ++row) {
      std::memcpy(result.data.data() + row * bytesPerRow,
                  bitmapPixels.data() + row * bitmapInfo.stride, bytesPerRow);
    }

    bitmapPixels.unlock();

    return result;
  }

  ImageData createImageBitmapFromData(
      std::span<const uint8_t> data) override {
    return decodeImageBitmapFromData(data);
  }

  void createImageBitmapFromDataAsync(
      std::span<const uint8_t> data, std::function<void(ImageData)> onSuccess,
      std::function<void(std::string)> onError) override {
    const auto decodeState = _decodeState;
    auto task = [decodeState,
                 ownedData = std::vector<uint8_t>(data.begin(), data.end()),
                 onSuccess = std::move(onSuccess), onError]() mutable {
      if (!decodeState || !decodeState->isActive()) {
        onError(kDecodeCancelledError);
        return;
      }
      try {
        auto result = decodeImageBitmapFromData(ownedData);
        if (!decodeState->isActive()) {
          onError(kDecodeCancelledError);
          return;
        }
        onSuccess(std::move(result));
      } catch (const std::exception &error) {
        onError(error.what());
      } catch (...) {
        onError("Unknown error while decoding image data");
      }
    };

    auto cancel = [onError]() {
      onError("Android image worker pool is shutting down");
    };
    if (!androidImageWorkerPool().submit(std::move(task), std::move(cancel))) {
      onError("Android image worker queue is full or shutting down");
    }
  }

  VideoFrameHandle loadVideoFrame(const std::string & /*path*/) override {
    // TODO: implement using MediaExtractor + MediaCodec to decode the first
    // frame into an AHardwareBuffer-backed Image (Android API 26+).
    throw std::runtime_error(
        "loadVideoFrame is not yet implemented on Android. Pass an "
        "AHardwareBuffer pointer obtained elsewhere (e.g. from "
        "react-native-vision-camera) directly to "
        "device.importSharedTextureMemory.");
  }

  VideoFrameHandle createTestVideoFrame(uint32_t width,
                                        uint32_t height) override {
    // Dawn's Android backend already requires API 26+, so AHardwareBuffer_*
    // symbols are guaranteed to be available at link time on supported builds.
    AHardwareBuffer_Desc desc = {};
    desc.width = width;
    desc.height = height;
    desc.layers = 1;
    desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                 AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY |
                 AHARDWAREBUFFER_USAGE_CPU_READ_RARELY;

    AHardwareBuffer *buffer = nullptr;
    int err = AHardwareBuffer_allocate(&desc, &buffer);
    if (err != 0 || !buffer) {
      throw std::runtime_error(
          "createTestVideoFrame: AHardwareBuffer_allocate failed (" +
          std::to_string(err) + ")");
    }

    AHardwareBuffer_Desc actualDesc = {};
    AHardwareBuffer_describe(buffer, &actualDesc);
    const uint32_t stridePixels = actualDesc.stride;

    void *vaddr = nullptr;
    int rc = AHardwareBuffer_lock(
        buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, nullptr, &vaddr);
    if (rc != 0 || !vaddr) {
      AHardwareBuffer_release(buffer);
      throw std::runtime_error(
          "createTestVideoFrame: AHardwareBuffer_lock failed (" +
          std::to_string(rc) + ")");
    }

    // Same procedural pattern as ApplePlatformContext::createTestVideoFrame so
    // the shared snapshot matches on both platforms. Apple writes BGRA bytes
    // (CVPixelBuffer is kCVPixelFormatType_32BGRA, sampled as bgra8unorm);
    // here we write RGBA bytes (AHardwareBuffer is R8G8B8A8_UNORM, sampled as
    // rgba8unorm). The fragment shader sees the same logical (r, g, b, a).
    uint8_t *base = static_cast<uint8_t *>(vaddr);
    const size_t rowBytes = stridePixels * 4;
    for (uint32_t y = 0; y < height; ++y) {
      uint8_t *row = base + y * rowBytes;
      for (uint32_t x = 0; x < width; ++x) {
        uint8_t r = static_cast<uint8_t>((x * 255) / std::max(width - 1, 1u));
        uint8_t g = static_cast<uint8_t>((y * 255) / std::max(height - 1, 1u));
        uint8_t b = static_cast<uint8_t>(((x + y) & 0x20) ? 220 : 30);
        row[x * 4 + 0] = r;
        row[x * 4 + 1] = g;
        row[x * 4 + 2] = b;
        row[x * 4 + 3] = 0xFF;
      }
    }
    AHardwareBuffer_unlock(buffer, nullptr);

    VideoFrameHandle handle;
    handle.handle = static_cast<void *>(buffer);
    handle.width = width;
    handle.height = height;
    handle.deleter = [buffer]() { AHardwareBuffer_release(buffer); };
    return handle;
  }

  std::unique_ptr<IVideoPlayer>
  createVideoPlayer(const std::string & /*path*/,
                    VideoPixelFormat /*format*/) override {
    // TODO: implement using MediaCodec -> ImageReader (AHardwareBuffer mode).
    throw std::runtime_error(
        "createVideoPlayer is not yet implemented on Android.");
  }

  std::string writeTestVideoFile() override {
    // TODO: implement using MediaCodec (H.264 encoder) or MediaMuxer.
    throw std::runtime_error(
        "writeTestVideoFile is not yet implemented on Android.");
  }

  VideoFrameHandle wrapNativeBuffer(void *pointer) override {
    if (!pointer) {
      throw std::runtime_error("wrapNativeBuffer: pointer is null");
    }
    auto *buffer = static_cast<AHardwareBuffer *>(pointer);

    AHardwareBuffer_Desc desc = {};
    AHardwareBuffer_describe(buffer, &desc);

    // Dawn derives the importable WebGPU usages from the AHB's usage bits;
    // without GPU_SAMPLED_IMAGE the imported texture never gets
    // TextureBinding and import fails deep inside Dawn with an opaque
    // validation error. Surface the real cause here instead. (CameraX's
    // default ImageReaders allocate CPU-only buffers; with
    // react-native-vision-camera, use pixelFormat: 'native' which allocates
    // GPU-sampleable buffers.)
    if ((desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) == 0) {
      throw std::runtime_error(
          "wrapNativeBuffer: this AHardwareBuffer was allocated without "
          "AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, so the GPU cannot sample "
          "it. Camera/CPU pipelines must allocate frames with GPU usage (for "
          "react-native-vision-camera, use pixelFormat: 'native').");
    }

    AHardwareBuffer_acquire(buffer);

    VideoFrameHandle handle;
    handle.handle = static_cast<void *>(buffer);
    handle.width = desc.width;
    handle.height = desc.height;
    // YUV / opaque formats route through Vulkan's SamplerYcbcrConversion via
    // Dawn's OpaqueYCbCrAndroidForExternalTexture path. Single-plane RGBA AHBs
    // take the plain BGRA8 path (sampled as a regular 2D texture).
    switch (desc.format) {
    case AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420:
    case AHARDWAREBUFFER_FORMAT_YCbCr_P010:
      handle.pixelFormat = VideoPixelFormat::NV12;
      break;
    default:
      handle.pixelFormat = VideoPixelFormat::BGRA8;
      break;
    }
    handle.deleter = [buffer]() { AHardwareBuffer_release(buffer); };
    return handle;
  }
};

} // namespace rnwgpu
