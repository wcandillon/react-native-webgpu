#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <ReactCommon/CallInvokerHolder.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <webgpu/webgpu_cpp.h>

#include "AndroidPlatformContext.h"
#include "RNWebGPUManager.h"
#include "SurfaceRegistry.h"

#define LOG_TAG "WebGPUModule"

namespace {

constexpr jint kSurfacePublishFailed = 0;
constexpr jint kSurfacePublishRegisteredOffscreen = 1;
constexpr jint kSurfacePublishOnscreen = 2;

rnwgpu::RNWebGPUSessionId sessionIdFromJava(jlong sessionId) noexcept {
  if (sessionId <= 0 || static_cast<rnwgpu::RNWebGPUSessionId>(sessionId) >
                            rnwgpu::kMaxRNWebGPUSessionId) {
    return rnwgpu::kInvalidRNWebGPUSessionId;
  }
  return static_cast<rnwgpu::RNWebGPUSessionId>(sessionId);
}

rnwgpu::SurfaceOwnerId surfaceOwnerIdFromJava(jlong ownerId) noexcept {
  if (ownerId <= 0) {
    return rnwgpu::kInvalidSurfaceOwnerId;
  }
  return static_cast<rnwgpu::SurfaceOwnerId>(ownerId);
}

jlong sessionIdToJava(rnwgpu::RNWebGPUSessionId sessionId) {
  if (sessionId == rnwgpu::kInvalidRNWebGPUSessionId ||
      sessionId > static_cast<rnwgpu::RNWebGPUSessionId>(
                      std::numeric_limits<jlong>::max())) {
    throw std::overflow_error(
        "WebGPU session ID cannot be represented by Java");
  }
  return static_cast<jlong>(sessionId);
}

void logError(const char *operation, const char *message) noexcept {
  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s failed: %s", operation,
                      message);
}

void throwJavaRuntimeException(JNIEnv *env, const char *message) noexcept {
  jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
  if (exceptionClass == nullptr) {
    return;
  }
  env->ThrowNew(exceptionClass, message);
  env->DeleteLocalRef(exceptionClass);
}

int surfaceDimension(jfloat value) noexcept {
  if (!std::isfinite(value) || value <= 0.0F) {
    return 0;
  }
  constexpr auto maxDimension =
      static_cast<jfloat>(std::numeric_limits<int>::max());
  return value >= maxDimension ? std::numeric_limits<int>::max()
                               : static_cast<int>(value);
}

} // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_com_webgpu_WebGPUModule_initializeNative(JNIEnv *env, jobject /*thiz*/,
                                              jlong jsRuntime,
                                              jobject jsCallInvokerHolder,
                                              jobject blobModule) {
  rnwgpu::RNWebGPUSessionId createdSessionId =
      rnwgpu::kInvalidRNWebGPUSessionId;
  try {
    if (jsRuntime == 0 || jsCallInvokerHolder == nullptr ||
        blobModule == nullptr) {
      throw std::invalid_argument(
          "A runtime, CallInvoker, and BlobModule are required");
    }

    auto *runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
    auto &registry = rnwgpu::RNWebGPUManagerRegistry::getInstance();

    const auto runtimeSessionId =
        rnwgpu::RNWebGPUManager::sessionForRuntime(*runtime);
    if (runtimeSessionId != rnwgpu::kInvalidRNWebGPUSessionId) {
      auto existingSession = registry.acquire(runtimeSessionId);
      if (existingSession) {
        return sessionIdToJava(existingSession.sessionId);
      }
    }

    auto jsCallInvoker =
        facebook::jni::alias_ref<
            facebook::react::CallInvokerHolder::javaobject>{
            reinterpret_cast<facebook::react::CallInvokerHolder::javaobject>(
                jsCallInvokerHolder)}
            ->cthis()
            ->getCallInvoker();
    if (!jsCallInvoker) {
      throw std::runtime_error("React Native returned an empty JS CallInvoker");
    }

    auto globalBlobModule = facebook::jni::make_global(
        facebook::jni::alias_ref<jobject>{blobModule});
    auto platformContext = std::make_shared<rnwgpu::AndroidPlatformContext>(
        std::move(globalBlobModule));

    createdSessionId = registry.createSession();
    const auto javaSessionId = sessionIdToJava(createdSessionId);
    auto manager = std::make_shared<rnwgpu::RNWebGPUManager>(
        createdSessionId, runtime, std::move(jsCallInvoker),
        std::move(platformContext));
    registry.publish(createdSessionId, std::move(manager));
    return javaSessionId;
  } catch (const std::exception &error) {
    if (createdSessionId != rnwgpu::kInvalidRNWebGPUSessionId) {
      rnwgpu::SurfaceRegistry::getInstance().closeSession(createdSessionId);
    }
    logError("initializeNative", error.what());
    throwJavaRuntimeException(env, error.what());
  } catch (...) {
    if (createdSessionId != rnwgpu::kInvalidRNWebGPUSessionId) {
      rnwgpu::SurfaceRegistry::getInstance().closeSession(createdSessionId);
    }
    constexpr auto message = "Unknown native WebGPU initialization error";
    logError("initializeNative", message);
    throwJavaRuntimeException(env, message);
  }
  return 0;
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_invalidateNative(
    JNIEnv * /*env*/, jobject /*thiz*/, jlong javaSessionId) {
  const auto sessionId = sessionIdFromJava(javaSessionId);
  if (sessionId == rnwgpu::kInvalidRNWebGPUSessionId) {
    return;
  }

  try {
    auto manager =
        rnwgpu::RNWebGPUManagerRegistry::getInstance().release(sessionId);
    manager.reset();
  } catch (const std::exception &error) {
    logError("invalidateNative", error.what());
  } catch (...) {
    logError("invalidateNative", "Unknown native WebGPU teardown error");
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceChanged(
    JNIEnv * /*env*/, jobject /*thiz*/, jlong javaSessionId,
    jlong javaSurfaceOwnerId, jobject /*surface*/, jint contextId, jfloat width,
    jfloat height) {
  try {
    const auto sessionId = sessionIdFromJava(javaSessionId);
    const auto surfaceOwnerId = surfaceOwnerIdFromJava(javaSurfaceOwnerId);
    if (surfaceOwnerId == rnwgpu::kInvalidSurfaceOwnerId || contextId <= 0 ||
        !rnwgpu::RNWebGPUManagerRegistry::getInstance().get(sessionId)) {
      return;
    }

    auto surfaceInfo =
        rnwgpu::SurfaceRegistry::getInstance().getSurfaceInfoIfOwnedBy(
            sessionId, contextId, surfaceOwnerId);
    if (surfaceInfo) {
      surfaceInfo->resizeIfOwnedBy(surfaceOwnerId, surfaceDimension(width),
                                   surfaceDimension(height));
    }
  } catch (const std::exception &error) {
    logError("onSurfaceChanged", error.what());
  } catch (...) {
    logError("onSurfaceChanged", "Unknown native surface resize error");
  }
}

extern "C" JNIEXPORT jint JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject /*thiz*/, jlong javaSessionId,
    jlong javaSurfaceOwnerId, jobject javaSurface, jint contextId, jfloat width,
    jfloat height) {
  const auto surfaceOwnerId = surfaceOwnerIdFromJava(javaSurfaceOwnerId);
  std::shared_ptr<rnwgpu::SurfaceInfo> surfaceInfo;
  try {
    const auto sessionId = sessionIdFromJava(javaSessionId);
    auto managerSnapshot =
        rnwgpu::RNWebGPUManagerRegistry::getInstance().get(sessionId);
    if (!managerSnapshot || surfaceOwnerId == rnwgpu::kInvalidSurfaceOwnerId ||
        contextId <= 0 || javaSurface == nullptr) {
      return kSurfacePublishFailed;
    }

    auto *nativeWindow = ANativeWindow_fromSurface(env, javaSurface);
    if (nativeWindow == nullptr) {
      logError("onSurfaceCreate", "ANativeWindow_fromSurface returned null");
      return kSurfacePublishFailed;
    }

    auto windowOwner =
        std::shared_ptr<void>(nativeWindow, [](void *window) noexcept {
          if (window != nullptr) {
            ANativeWindow_release(static_cast<ANativeWindow *>(window));
          }
        });

    auto &surfaceRegistry = rnwgpu::SurfaceRegistry::getInstance();
    const int nativeWidth = surfaceDimension(width);
    const int nativeHeight = surfaceDimension(height);
    auto webGpuSurface = managerSnapshot.manager->_platformContext->makeSurface(
        managerSnapshot.manager->_gpu, nativeWindow, nativeWidth, nativeHeight);
    if (!webGpuSurface) {
      throw std::runtime_error("WebGPU surface creation returned null");
    }
    if (!managerSnapshot.manager->isActive()) {
      return kSurfacePublishFailed;
    }

    surfaceInfo = surfaceRegistry.claimSurfaceInfo(
        sessionId, contextId, surfaceOwnerId, managerSnapshot.manager->_gpu,
        nativeWidth, nativeHeight);
    if (!surfaceInfo) {
      return kSurfacePublishFailed;
    }

    const bool switched = surfaceInfo->switchToOnscreenIfOwnedBy(
        surfaceOwnerId, nativeWindow, std::move(webGpuSurface), windowOwner);
    if (!switched) {
      return kSurfacePublishFailed;
    }
    // The session can be invalidated while Dawn creates the surface. If that
    // happened after claimSurfaceInfo(), do not leave an unregistered onscreen
    // surface alive through the remainder of reload teardown.
    if (!managerSnapshot.manager->isActive()) {
      (void)surfaceRegistry.removeSurfaceInfoIfOwnedBy(
          sessionId, contextId, surfaceOwnerId, surfaceInfo);
      return kSurfacePublishFailed;
    }
    return kSurfacePublishOnscreen;
  } catch (const std::exception &error) {
    const bool registeredOffscreen =
        surfaceInfo && surfaceInfo->unconfigureIfOwnedBy(surfaceOwnerId);
    logError("onSurfaceCreate", error.what());
    return registeredOffscreen ? kSurfacePublishRegisteredOffscreen
                               : kSurfacePublishFailed;
  } catch (...) {
    const bool registeredOffscreen =
        surfaceInfo && surfaceInfo->unconfigureIfOwnedBy(surfaceOwnerId);
    logError("onSurfaceCreate", "Unknown WebGPU surface creation error");
    return registeredOffscreen ? kSurfacePublishRegisteredOffscreen
                               : kSurfacePublishFailed;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUView_switchToOffscreenSurface(JNIEnv * /*env*/,
                                                    jobject /*thiz*/,
                                                    jlong javaSessionId,
                                                    jlong javaSurfaceOwnerId,
                                                    jint contextId) {
  try {
    const auto sessionId = sessionIdFromJava(javaSessionId);
    const auto surfaceOwnerId = surfaceOwnerIdFromJava(javaSurfaceOwnerId);
    if (surfaceOwnerId == rnwgpu::kInvalidSurfaceOwnerId || contextId <= 0 ||
        !rnwgpu::RNWebGPUManagerRegistry::getInstance().get(sessionId)) {
      return;
    }

    auto surfaceInfo =
        rnwgpu::SurfaceRegistry::getInstance().getSurfaceInfoIfOwnedBy(
            sessionId, contextId, surfaceOwnerId);
    if (surfaceInfo) {
      surfaceInfo->switchToOffscreenIfOwnedBy(surfaceOwnerId);
    }
  } catch (const std::exception &error) {
    logError("switchToOffscreenSurface", error.what());
  } catch (...) {
    logError("switchToOffscreenSurface",
             "Unknown native offscreen transition error");
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceDestroy(
    JNIEnv * /*env*/, jobject /*thiz*/, jlong javaSessionId,
    jlong javaSurfaceOwnerId, jint contextId) {
  try {
    const auto sessionId = sessionIdFromJava(javaSessionId);
    const auto surfaceOwnerId = surfaceOwnerIdFromJava(javaSurfaceOwnerId);
    if (sessionId == rnwgpu::kInvalidRNWebGPUSessionId ||
        surfaceOwnerId == rnwgpu::kInvalidSurfaceOwnerId || contextId <= 0) {
      return;
    }
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    auto surfaceInfo =
        registry.getSurfaceInfoIfOwnedBy(sessionId, contextId, surfaceOwnerId);
    if (!surfaceInfo) {
      return;
    }

    (void)registry.removeSurfaceInfoIfOwnedBy(sessionId, contextId,
                                              surfaceOwnerId, surfaceInfo);
  } catch (const std::exception &error) {
    logError("onSurfaceDestroy", error.what());
  } catch (...) {
    logError("onSurfaceDestroy", "Unknown native surface teardown error");
  }
}
