#pragma once

#include <android/bitmap.h>
#include <jni.h>

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"
#include "RNWebGPUManager.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;
namespace jni = facebook::jni;

class AndroidPlatformContext : public PlatformContext {
public:
  AndroidPlatformContext() = default;
  ~AndroidPlatformContext() = default;

  wgpu::Surface makeSurface(wgpu::Instance instance, void *window, int width,
                            int height) override {
    wgpu::SurfaceDescriptorFromAndroidNativeWindow androidSurfaceDesc;
    androidSurfaceDesc.window = reinterpret_cast<ANativeWindow *>(window);
    wgpu::SurfaceDescriptor surfaceDescriptor;
    surfaceDescriptor.nextInChain = &androidSurfaceDesc;
    return instance.CreateSurface(&surfaceDescriptor);
  }

  ImageData createImageBitmap(std::string blobId, double offset,
                              double size) override {
    jni::Environment::ensureCurrentThreadIsAttached();

    jni::ThreadScope ts; // Manages JNI thread attachment/detachment
                         // Get the JNI environment
    JNIEnv *env = facebook::jni::Environment::current();
    if (!env) {
      throw std::runtime_error("Couldn't get JNI environment");
    }

    // Get the BlobModule class and its resolve method
    jclass blobModuleClass =
        env->FindClass("com/facebook/react/modules/blob/BlobModule");
    if (!blobModuleClass) {
      throw std::runtime_error("Couldn't find BlobModule class");
    }

    jmethodID resolveMethod = env->GetStaticMethodID(
        blobModuleClass, "resolve", "(Ljava/lang/String;JJ)[B");
    if (!resolveMethod) {
      throw std::runtime_error("Couldn't find resolve method in BlobModule");
    }

    // Resolve the blob data
    jstring jBlobId = env->NewStringUTF(blobId.c_str());
    jbyteArray blobData = (jbyteArray)env->CallStaticObjectMethod(
        blobModuleClass, resolveMethod, jBlobId, (jlong)offset, (jlong)size);
    env->DeleteLocalRef(jBlobId);

    if (!blobData) {
      throw std::runtime_error("Couldn't retrieve blob data");
    }

    // Create a Bitmap from the blob data
    jclass bitmapFactoryClass =
        env->FindClass("android/graphics/BitmapFactory");
    jmethodID decodeByteArrayMethod =
        env->GetStaticMethodID(bitmapFactoryClass, "decodeByteArray",
                               "([BII)Landroid/graphics/Bitmap;");
    jint blobLength = env->GetArrayLength(blobData);
    jobject bitmap = env->CallStaticObjectMethod(
        bitmapFactoryClass, decodeByteArrayMethod, blobData, 0, blobLength);

    if (!bitmap) {
      env->DeleteLocalRef(blobData);
      throw std::runtime_error("Couldn't decode image");
    }

    // Get bitmap info
    AndroidBitmapInfo bitmapInfo;
    if (AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) !=
        ANDROID_BITMAP_RESULT_SUCCESS) {
      env->DeleteLocalRef(blobData);
      env->DeleteLocalRef(bitmap);
      throw std::runtime_error("Couldn't get bitmap info");
    }

    // Lock the bitmap pixels
    void *bitmapPixels;
    if (AndroidBitmap_lockPixels(env, bitmap, &bitmapPixels) !=
        ANDROID_BITMAP_RESULT_SUCCESS) {
      env->DeleteLocalRef(blobData);
      env->DeleteLocalRef(bitmap);
      throw std::runtime_error("Couldn't lock bitmap pixels");
    }

    // Copy the bitmap data
    std::vector<uint8_t> imageData(bitmapInfo.height * bitmapInfo.stride);
    memcpy(imageData.data(), bitmapPixels, imageData.size());

    // Unlock the bitmap pixels
    AndroidBitmap_unlockPixels(env, bitmap);

    // Clean up JNI references
    env->DeleteLocalRef(blobData);
    env->DeleteLocalRef(bitmap);

    ImageData result;
    result.width = static_cast<int>(bitmapInfo.width);
    result.height = static_cast<int>(bitmapInfo.height);
    result.data = imageData.data();
    result.size = imageData.size();
    return result;
  }
};

} // namespace rnwgpu
