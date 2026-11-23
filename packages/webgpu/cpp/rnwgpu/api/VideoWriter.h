#pragma once

#include <memory>
#include <string>
#include <utility>

#include <ReactCommon/CallInvoker.h>

#include "FenceArgument.h"
#include "JSIConverter.h"
#include "NativeObject.h"
#include "NativeVideoWriter.h"
#include "Promise.h"
#include "VideoFrame.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// The JS VideoWriter (see src/video.ts): dequeueBuffer(), append(), finish()
// and cancel() over a platform IVideoWriter. Its promises settle on the main
// JS runtime, through the call invoker.
class VideoWriter : public NativeObject<VideoWriter> {
public:
  static constexpr const char *CLASS_NAME = "VideoWriter";

  VideoWriter(std::unique_ptr<IVideoWriter> impl,
              std::shared_ptr<facebook::react::CallInvoker> callInvoker,
              std::string path)
      : NativeObject(CLASS_NAME), _impl(std::move(impl)),
        _callInvoker(std::move(callInvoker)), _path(std::move(path)) {}

  std::string getBrand() { return CLASS_NAME; }

  // The absolute path of the file written.
  std::string getPath() { return _path; }

  // A JS Error with `code`, as the writer's promises reject with and its
  // methods throw.
  static jsi::JSError makeError(jsi::Runtime &runtime, const std::string &code,
                                const std::string &message) {
    jsi::JSError error(runtime, message);
    if (error.value().isObject()) {
      error.value().getObject(runtime).setProperty(
          runtime, "code", jsi::String::createFromUtf8(runtime, code));
    }
    return error;
  }

  // Rejects `promise` with a writer error, on the JS thread.
  static VideoWriterErrorCallback
  rejectOn(std::shared_ptr<facebook::react::CallInvoker> callInvoker,
           std::shared_ptr<Promise> promise) {
    return [callInvoker = std::move(callInvoker), promise = std::move(promise)](
               const std::string &code, const std::string &message) {
      callInvoker->invokeAsync([promise, code, message]() {
        promise->reject(makeError(promise->runtime, code, message));
      });
    };
  }

  jsi::Value dequeueBuffer(jsi::Runtime &runtime,
                           const jsi::Value & /*thisVal*/,
                           const jsi::Value * /*args*/, size_t /*count*/) {
    auto callInvoker = _callInvoker;
    auto impl = _impl;
    return Promise::createPromise(
        runtime, [impl, callInvoker](jsi::Runtime & /*runtime*/,
                                     std::shared_ptr<Promise> promise) {
          impl->dequeueBuffer(
              [callInvoker, promise](VideoFrameHandle handle) {
                auto frame = std::make_shared<VideoFrame>(std::move(handle));
                callInvoker->invokeAsync([promise, frame]() {
                  promise->resolve(VideoFrame::create(promise->runtime, frame));
                });
              },
              rejectOn(callInvoker, promise));
        });
  }

  // append(buffer, done, timestamp): `done` is what
  // GPUSharedTextureMemory.endAccess() returned ({ fences }), a list of fence
  // states, a single one, or null for a buffer already complete.
  jsi::Value append(jsi::Runtime &runtime, const jsi::Value & /*thisVal*/,
                    const jsi::Value *args, size_t count) {
    if (count < 3 || !args[0].isObject() || !args[2].isNumber()) {
      throw makeError(runtime, "invalid-argument",
                      "VideoWriter.append(buffer, done, timestamp): expected "
                      "a NativeBuffer, the end-access state of its drawing, "
                      "and a timestamp in seconds");
    }
    auto frame = JSIConverter<std::shared_ptr<VideoFrame>>::fromJSI(
        runtime, args[0], false);
    if (!frame || frame->getHandle() == nullptr) {
      throw makeError(runtime, "invalid-argument",
                      "VideoWriter.append(): the buffer is not a NativeBuffer, "
                      "or was released");
    }
    wgpu::SharedFence fence;
    uint64_t signaledValue = 0;
    try {
      fenceFromArgument(runtime, args[1], "VideoWriter.append", &fence,
                        &signaledValue);
    } catch (const std::invalid_argument &e) {
      throw makeError(runtime, "invalid-argument", e.what());
    }
    try {
      _impl->append(frame->getHandle(), args[2].asNumber(), std::move(fence),
                    signaledValue);
    } catch (const VideoWriterError &e) {
      throw makeError(runtime, e.code(), e.what());
    }
    // The writer holds the buffer from here on.
    frame->release();
    return jsi::Value::undefined();
  }

  jsi::Value finish(jsi::Runtime &runtime, const jsi::Value & /*thisVal*/,
                    const jsi::Value * /*args*/, size_t /*count*/) {
    auto callInvoker = _callInvoker;
    auto impl = _impl;
    return Promise::createPromise(
        runtime, [impl, callInvoker](jsi::Runtime & /*runtime*/,
                                     std::shared_ptr<Promise> promise) {
          impl->finish(
              [callInvoker, promise]() {
                callInvoker->invokeAsync(
                    [promise]() { promise->resolve(jsi::Value::undefined()); });
              },
              rejectOn(callInvoker, promise));
        });
  }

  // Resolves once the partial file is deleted: a new writer can then use the
  // same path.
  jsi::Value cancel(jsi::Runtime &runtime, const jsi::Value & /*thisVal*/,
                    const jsi::Value * /*args*/, size_t /*count*/) {
    auto callInvoker = _callInvoker;
    auto impl = _impl;
    return Promise::createPromise(
        runtime, [impl, callInvoker](jsi::Runtime & /*runtime*/,
                                     std::shared_ptr<Promise> promise) {
          impl->cancel([callInvoker, promise]() {
            callInvoker->invokeAsync(
                [promise]() { promise->resolve(jsi::Value::undefined()); });
          });
        });
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &VideoWriter::getBrand);
    installGetter(runtime, prototype, "path", &VideoWriter::getPath);
    installMethod(runtime, prototype, "dequeueBuffer",
                  &VideoWriter::dequeueBuffer);
    installMethod(runtime, prototype, "append", &VideoWriter::append);
    installMethod(runtime, prototype, "finish", &VideoWriter::finish);
    installMethod(runtime, prototype, "cancel", &VideoWriter::cancel);
  }

private:
  // Shared with the callbacks of pending calls, which may outlive the JS
  // object.
  std::shared_ptr<IVideoWriter> _impl;
  std::shared_ptr<facebook::react::CallInvoker> _callInvoker;
  std::string _path;
};

} // namespace rnwgpu
