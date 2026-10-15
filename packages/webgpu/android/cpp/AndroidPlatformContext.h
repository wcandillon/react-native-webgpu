#pragma once

#include <android/bitmap.h>
#include <android/data_space.h>
#include <android/hardware_buffer.h>
#include <dlfcn.h>
#include <jni.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "AndroidColorSpace.h"
#include "AndroidVideoPlayer.h"
#include "PlatformContext.h"
#include "RNWebGPUManager.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;
namespace jni = facebook::jni;

// The completion callbacks of one snapshotView request, heap-allocated and
// carried through Java as an opaque pointer (WebGPUModule.snapshotView ->
// onViewSnapshot), which hands them back exactly once.
struct ViewSnapshotCallbacks {
  std::function<void(ImageData)> onSuccess;
  std::function<void(std::string)> onError;
};

class AndroidPlatformContext : public PlatformContext {
private:
  jobject _blobModule;
  // Global ref to the com.webgpu.WebGPUModule instance (view snapshots).
  jobject _module;

  std::vector<uint8_t> resolveBlob(JNIEnv *env, const std::string &blobId,
                                   double offset, double size) {
    if (!_blobModule) {
      throw std::runtime_error("BlobModule instance is null");
    }

    jclass blobModuleClass = env->GetObjectClass(_blobModule);
    if (!blobModuleClass) {
      throw std::runtime_error("Couldn't find BlobModule class");
    }

    jmethodID resolveMethod = env->GetMethodID(blobModuleClass, "resolve",
                                               "(Ljava/lang/String;II)[B");
    env->DeleteLocalRef(blobModuleClass);

    if (!resolveMethod) {
      throw std::runtime_error("Couldn't find resolve method in BlobModule");
    }

    jstring jBlobId = env->NewStringUTF(blobId.c_str());
    jbyteArray blobData = (jbyteArray)env->CallObjectMethod(
        _blobModule, resolveMethod, jBlobId, static_cast<jint>(offset),
        static_cast<jint>(size));
    env->DeleteLocalRef(jBlobId);

    if (!blobData) {
      throw std::runtime_error("Couldn't retrieve blob data");
    }

    jsize len = env->GetArrayLength(blobData);
    std::vector<uint8_t> data(len);
    env->GetByteArrayRegion(blobData, 0, len,
                            reinterpret_cast<jbyte *>(data.data()));
    env->DeleteLocalRef(blobData);
    return data;
  }

public:
  // Both arguments are global refs; the context releases them.
  AndroidPlatformContext(jobject blobModule, jobject module)
      : _blobModule(blobModule), _module(module) {}
  ~AndroidPlatformContext() {
    JNIEnv *env = facebook::jni::Environment::current();
    if (_blobModule) {
      env->DeleteGlobalRef(_blobModule);
      _blobModule = nullptr;
    }
    if (_module) {
      env->DeleteGlobalRef(_module);
      _module = nullptr;
    }
  }

  void snapshotView(const ViewSnapshotRequest &request,
                    std::function<void(ImageData)> onSuccess,
                    std::function<void(std::string)> onError) override {
    if (!_module) {
      onError("drawElementImageToTexture: the WebGPU module is gone");
      return;
    }
    jni::Environment::ensureCurrentThreadIsAttached();
    JNIEnv *env = facebook::jni::Environment::current();
    if (!env) {
      onError("drawElementImageToTexture: couldn't get the JNI environment");
      return;
    }
    jclass moduleClass = env->GetObjectClass(_module);
    jmethodID method =
        env->GetMethodID(moduleClass, "snapshotView", "(IDDDDIIJ)V");
    env->DeleteLocalRef(moduleClass);
    if (!method) {
      env->ExceptionClear();
      onError("drawElementImageToTexture: WebGPUModule.snapshotView not found");
      return;
    }
    // Ownership passes to Java; Java_com_webgpu_WebGPUModule_onViewSnapshot
    // (cpp-adapter.cpp) reclaims it.
    auto *callbacks =
        new ViewSnapshotCallbacks{std::move(onSuccess), std::move(onError)};
    env->CallVoidMethod(_module, method, static_cast<jint>(request.viewTag),
                        static_cast<jdouble>(request.sourceX),
                        static_cast<jdouble>(request.sourceY),
                        static_cast<jdouble>(request.sourceWidth),
                        static_cast<jdouble>(request.sourceHeight),
                        static_cast<jint>(request.width),
                        static_cast<jint>(request.height),
                        reinterpret_cast<jlong>(callbacks));
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      auto fail = std::move(callbacks->onError);
      delete callbacks;
      fail("drawElementImageToTexture: WebGPUModule.snapshotView threw");
    }
  }

  ImageData createImageBitmap(std::string blobId, double offset,
                              double size) override {
    jni::Environment::ensureCurrentThreadIsAttached();

    JNIEnv *env = facebook::jni::Environment::current();
    if (!env) {
      throw std::runtime_error("Couldn't get JNI environment");
    }

    auto data = resolveBlob(env, blobId, offset, size);
    return createImageBitmapFromData(data);
  }

  void
  createImageBitmapAsync(std::string blobId, double offset, double size,
                         std::function<void(ImageData)> onSuccess,
                         std::function<void(std::string)> onError) override {
    std::thread([this, blobId = std::move(blobId), offset, size,
                 onSuccess = std::move(onSuccess),
                 onError = std::move(onError)]() {
      jni::Environment::ensureCurrentThreadIsAttached();
      try {
        JNIEnv *env = facebook::jni::Environment::current();
        if (!env) {
          throw std::runtime_error("Couldn't get JNI environment");
        }
        auto data = resolveBlob(env, blobId, offset, size);
        auto result = createImageBitmapFromData(data);
        onSuccess(std::move(result));
      } catch (const std::exception &e) {
        onError(e.what());
      }
    }).detach();
  }

  ImageData createImageBitmapFromData(std::span<const uint8_t> data) override {
    jni::Environment::ensureCurrentThreadIsAttached();

    JNIEnv *env = facebook::jni::Environment::current();
    if (!env) {
      throw std::runtime_error("Couldn't get JNI environment");
    }

    // Create jbyteArray from the raw bytes
    jbyteArray byteArray = env->NewByteArray(static_cast<jsize>(data.size()));
    if (!byteArray) {
      throw std::runtime_error("Couldn't allocate byte array");
    }
    env->SetByteArrayRegion(byteArray, 0, static_cast<jsize>(data.size()),
                            reinterpret_cast<const jbyte *>(data.data()));

    // Decode via BitmapFactory
    jclass bitmapFactoryClass =
        env->FindClass("android/graphics/BitmapFactory");
    if (!bitmapFactoryClass) {
      env->DeleteLocalRef(byteArray);
      throw std::runtime_error("Couldn't find BitmapFactory class");
    }
    jmethodID decodeByteArrayMethod =
        env->GetStaticMethodID(bitmapFactoryClass, "decodeByteArray",
                               "([BII)Landroid/graphics/Bitmap;");
    if (!decodeByteArrayMethod) {
      env->DeleteLocalRef(byteArray);
      env->DeleteLocalRef(bitmapFactoryClass);
      throw std::runtime_error("Couldn't find decodeByteArray method");
    }
    jint length = static_cast<jint>(data.size());
    jobject bitmap = env->CallStaticObjectMethod(
        bitmapFactoryClass, decodeByteArrayMethod, byteArray, 0, length);
    env->DeleteLocalRef(bitmapFactoryClass);

    if (!bitmap) {
      env->DeleteLocalRef(byteArray);
      throw std::runtime_error("Couldn't decode image");
    }

    AndroidBitmapInfo bitmapInfo;
    if (AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) !=
        ANDROID_BITMAP_RESULT_SUCCESS) {
      env->DeleteLocalRef(byteArray);
      env->DeleteLocalRef(bitmap);
      throw std::runtime_error("Couldn't get bitmap info");
    }

    void *bitmapPixels;
    if (AndroidBitmap_lockPixels(env, bitmap, &bitmapPixels) !=
        ANDROID_BITMAP_RESULT_SUCCESS) {
      env->DeleteLocalRef(byteArray);
      env->DeleteLocalRef(bitmap);
      throw std::runtime_error("Couldn't lock bitmap pixels");
    }

    ImageData result;
    result.width = static_cast<int>(bitmapInfo.width);
    result.height = static_cast<int>(bitmapInfo.height);
    result.data.resize(bitmapInfo.height * bitmapInfo.stride);
    memcpy(result.data.data(), bitmapPixels, result.data.size());
    // BitmapFactory hands back premultiplied ARGB_8888 pixels;
    // createImageBitmap converts to the representation requested by
    // premultiplyAlpha.
    result.premultiplied = true;

    AndroidBitmap_unlockPixels(env, bitmap);

    env->DeleteLocalRef(byteArray);
    env->DeleteLocalRef(bitmap);

    return result;
  }

  void createImageBitmapFromDataAsync(
      std::span<const uint8_t> data, std::function<void(ImageData)> onSuccess,
      std::function<void(std::string)> onError) override {
    std::thread([this,
                 ownedData = std::vector<uint8_t>(data.begin(), data.end()),
                 onSuccess = std::move(onSuccess),
                 onError = std::move(onError)]() mutable {
      jni::Environment::ensureCurrentThreadIsAttached();
      try {
        auto result = createImageBitmapFromData(ownedData);
        onSuccess(std::move(result));
      } catch (const std::exception &e) {
        onError(e.what());
      }
    }).detach();
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

  // The NV12 variant of createTestVideoFrame: a Y8Cb8Cr8_420 buffer, the YUV
  // layout every gralloc supports, BT.709 limited range, red on the left half
  // and green on the right. Filling it needs AHardwareBuffer_lockPlanes
  // (API 29), resolved at run time since the library runs from API 26.
  VideoFrameHandle createTestYuvFrame(uint32_t width, uint32_t height) {
    using LockPlanes = int (*)(AHardwareBuffer *, uint64_t, int32_t,
                               const ARect *, AHardwareBuffer_Planes *);
    static const auto lockPlanes = reinterpret_cast<LockPlanes>(
        dlsym(RTLD_DEFAULT, "AHardwareBuffer_lockPlanes"));
    if (lockPlanes == nullptr) {
      throw std::runtime_error(
          "createTestVideoFrame: unsupported: a YUV test frame needs "
          "AHardwareBuffer_lockPlanes (API 29)");
    }

    AHardwareBuffer_Desc desc = {};
    desc.width = width;
    desc.height = height;
    desc.layers = 1;
    desc.format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                 AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY |
                 AHARDWAREBUFFER_USAGE_CPU_READ_RARELY;
    AHardwareBuffer *buffer = nullptr;
    int err = AHardwareBuffer_allocate(&desc, &buffer);
    if (err != 0 || !buffer) {
      throw std::runtime_error(
          "createTestVideoFrame: unsupported: AHardwareBuffer_allocate failed "
          "for a Y8Cb8Cr8_420 buffer (" +
          std::to_string(err) + ")");
    }

    AHardwareBuffer_Planes planes = {};
    int rc = lockPlanes(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1,
                        nullptr, &planes);
    if (rc != 0 || planes.planeCount < 3) {
      AHardwareBuffer_release(buffer);
      throw std::runtime_error(
          "createTestVideoFrame: unsupported: AHardwareBuffer_lockPlanes "
          "failed (" +
          std::to_string(rc) + ")");
    }
    // Plane 0 is Y, 1 is Cb, 2 is Cr; the chroma planes are subsampled by two
    // on both axes and the strides say whether Cb and Cr interleave.
    const auto &y = planes.planes[0];
    const auto &cb = planes.planes[1];
    const auto &cr = planes.planes[2];
    for (uint32_t row = 0; row < height; ++row) {
      auto *line = static_cast<uint8_t *>(y.data) + row * y.rowStride;
      for (uint32_t col = 0; col < width; ++col) {
        line[col * y.pixelStride] =
            col < width / 2 ? test_yuv_frame::kRedY : test_yuv_frame::kGreenY;
      }
    }
    const uint32_t chromaWidth = (width + 1) / 2;
    const uint32_t chromaHeight = (height + 1) / 2;
    for (uint32_t row = 0; row < chromaHeight; ++row) {
      auto *cbLine = static_cast<uint8_t *>(cb.data) + row * cb.rowStride;
      auto *crLine = static_cast<uint8_t *>(cr.data) + row * cr.rowStride;
      for (uint32_t col = 0; col < chromaWidth; ++col) {
        bool red = col < chromaWidth / 2;
        cbLine[col * cb.pixelStride] =
            red ? test_yuv_frame::kRedCb : test_yuv_frame::kGreenCb;
        crLine[col * cr.pixelStride] =
            red ? test_yuv_frame::kRedCr : test_yuv_frame::kGreenCr;
      }
    }
    AHardwareBuffer_unlock(buffer, nullptr);

    VideoFrameHandle handle;
    handle.handle = static_cast<void *>(buffer);
    handle.width = width;
    handle.height = height;
    handle.pixelFormat = VideoPixelFormat::NV12;
    // What a BT.709 SDR video declares: ADATASPACE_BT709 is STANDARD_BT709 |
    // TRANSFER_SMPTE_170M | RANGE_LIMITED.
    handle.colorSpace = colorSpaceFromDataSpace(ADATASPACE_BT709);
    handle.deleter = [buffer]() { AHardwareBuffer_release(buffer); };
    return handle;
  }

  VideoFrameHandle createTestVideoFrame(uint32_t width, uint32_t height,
                                        VideoPixelFormat format) override {
    if (format == VideoPixelFormat::NV12) {
      return createTestYuvFrame(width, height);
    }
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

  // The decoder writes its native YUV layout whatever format is requested:
  // the frames are sampled through importExternalTexture (see
  // WebGPUVideoPlayer.java).
  std::unique_ptr<IVideoPlayer>
  createVideoPlayer(const std::string &path,
                    VideoPixelFormat /*format*/) override {
    if (!_module) {
      throw std::runtime_error("createVideoPlayer: the WebGPU module is gone");
    }
    jni::Environment::ensureCurrentThreadIsAttached();
    JNIEnv *env = facebook::jni::Environment::current();
    if (!env) {
      throw std::runtime_error(
          "createVideoPlayer: couldn't get the JNI environment");
    }
    jclass moduleClass = env->GetObjectClass(_module);
    jmethodID method =
        env->GetMethodID(moduleClass, "createVideoPlayer",
                         "(Ljava/lang/String;)Lcom/webgpu/WebGPUVideoPlayer;");
    env->DeleteLocalRef(moduleClass);
    if (!method) {
      env->ExceptionClear();
      throw std::runtime_error(
          "createVideoPlayer: WebGPUModule.createVideoPlayer not found");
    }
    jstring jPath = env->NewStringUTF(path.c_str());
    jobject player = env->CallObjectMethod(_module, method, jPath);
    env->DeleteLocalRef(jPath);
    if (env->ExceptionCheck()) {
      throw std::runtime_error("createVideoPlayer: " +
                               describeAndClearJavaException(env));
    }
    if (player == nullptr) {
      throw std::runtime_error("createVideoPlayer: no player was created");
    }
    auto result = std::make_unique<AndroidVideoPlayer>(env, player);
    env->DeleteLocalRef(player);
    return result;
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
