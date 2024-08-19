#pragma once

#include <android/bitmap.h>
#include <jni.h>

#include <memory>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"
#include "RNWebGPUManager.h"

namespace rnwgpu {

    namespace jsi = facebook::jsi;
    namespace jni = facebook::jni;

    class AndroidPlatformContext : public PlatformContext {
    private:
        jobject _blobModule;
    public:
        AndroidPlatformContext(jobject blobModule): _blobModule(blobModule)  {
        }
        ~AndroidPlatformContext() {
          if (_blobModule) {
            JNIEnv *env = facebook::jni::Environment::current();
            env->DeleteGlobalRef(_blobModule);
            _blobModule = nullptr;
          }
        }

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

            JNIEnv *env = facebook::jni::Environment::current();
            if (!env) {
                throw std::runtime_error("Couldn't get JNI environment");
            }

            // Use the BlobModule instance from _blobModule
            if (!_blobModule) {
                throw std::runtime_error("BlobModule instance is null");
            }

            // Get the resolve method ID
            jclass blobModuleClass = env->GetObjectClass(_blobModule);
            if (!blobModuleClass) {
                throw std::runtime_error("Couldn't find BlobModule class");
            }

            jmethodID resolveMethod = env->GetMethodID(
                    blobModuleClass, "resolve", "(Ljava/lang/String;II)[B");
            if (!resolveMethod) {
                throw std::runtime_error("Couldn't find resolve method in BlobModule");
            }

            // Resolve the blob data
            jstring jBlobId = env->NewStringUTF(blobId.c_str());
            jbyteArray blobData = (jbyteArray)env->CallObjectMethod(
                    _blobModule, resolveMethod, jBlobId, static_cast<jint>(offset), static_cast<jint>(size));
            env->DeleteLocalRef(jBlobId);

            if (!blobData) {
                throw std::runtime_error("Couldn't retrieve blob data");
            }

            // Create a Bitmap from the blob data
            jclass bitmapFactoryClass = env->FindClass("android/graphics/BitmapFactory");
            jmethodID decodeByteArrayMethod = env->GetStaticMethodID(
                    bitmapFactoryClass, "decodeByteArray", "([BII)Landroid/graphics/Bitmap;");
            jint blobLength = env->GetArrayLength(blobData);
            jobject bitmap = env->CallStaticObjectMethod(
                    bitmapFactoryClass, decodeByteArrayMethod, blobData, 0, blobLength);

            if (!bitmap) {
                env->DeleteLocalRef(blobData);
                throw std::runtime_error("Couldn't decode image");
            }

            // Get bitmap info
            AndroidBitmapInfo bitmapInfo;
            if (AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) != ANDROID_BITMAP_RESULT_SUCCESS) {
                env->DeleteLocalRef(blobData);
                env->DeleteLocalRef(bitmap);
                throw std::runtime_error("Couldn't get bitmap info");
            }

            // Lock the bitmap pixels
            void *bitmapPixels;
            if (AndroidBitmap_lockPixels(env, bitmap, &bitmapPixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
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
            result.data = imageData;
            return result;
        }
    };

} // namespace rnwgpu
