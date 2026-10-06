#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Unions.h"

#include "NativeObject.h"

#include "rnwgpu/async/AsyncTaskHandle.h"
#include "rnwgpu/async/RuntimeContext.h"

#include "webgpu/webgpu_cpp.h"

#include "ArrayBuffer.h"
#include "GPUBuffer.h"
#include "GPUCommandBuffer.h"
#include "GPUDrawElementImageDestination.h"
#include "GPUDrawElementImageSource.h"
#include "GPUImageCopyExternalImage.h"
#include "GPUImageCopyTextureTagged.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUQueue : public NativeObject<GPUQueue> {
public:
  static constexpr const char *CLASS_NAME = "GPUQueue";

  explicit GPUQueue(wgpu::Queue instance,
                    std::shared_ptr<async::RuntimeContext> async,
                    std::string label)
      : NativeObject(CLASS_NAME), _instance(instance), _async(async),
        _label(label) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  void submit(std::vector<std::shared_ptr<GPUCommandBuffer>> commandBuffers);
  async::AsyncTaskHandle onSubmittedWorkDone(jsi::Runtime &runtime);
  void writeBuffer(std::shared_ptr<GPUBuffer> buffer, uint64_t bufferOffset,
                   std::shared_ptr<ArrayBuffer> data,
                   std::optional<uint64_t> dataOffsetElements,
                   std::optional<size_t> sizeElements);
  void writeTexture(std::shared_ptr<GPUImageCopyTexture> destination,
                    std::shared_ptr<ArrayBuffer> data,
                    std::shared_ptr<GPUImageDataLayout> dataLayout,
                    std::shared_ptr<GPUExtent3D> size);
  void copyExternalImageToTexture(
      std::shared_ptr<GPUImageCopyExternalImage> source,
      std::shared_ptr<GPUImageCopyTextureTagged> destination,
      std::shared_ptr<GPUExtent3D> copySize);

  // Non-spec React Native extension modeled on the html-in-canvas proposal's
  // GPUQueue.drawElementImageToTexture: rasterizes a native view (the
  // "element") and writes the pixels into `destination.texture`. Unlike the
  // web API it returns a Promise, because the view can only be rasterized on
  // the platform UI thread; the texture write is issued on the calling
  // runtime's thread right before the promise resolves, so work submitted
  // after `await` observes the new contents.
  async::AsyncTaskHandle drawElementImageToTexture(
      jsi::Runtime &runtime,
      std::shared_ptr<GPUDrawElementImageSource> source,
      std::shared_ptr<GPUDrawElementImageDestination> destination);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUQueue::getBrand);
    installMethod(runtime, prototype, "submit", &GPUQueue::submit);
    installMethodWithRuntime(runtime, prototype, "onSubmittedWorkDone",
                             &GPUQueue::onSubmittedWorkDone);
    installMethod(runtime, prototype, "writeBuffer", &GPUQueue::writeBuffer);
    installMethod(runtime, prototype, "writeTexture", &GPUQueue::writeTexture);
    installMethod(runtime, prototype, "copyExternalImageToTexture",
                  &GPUQueue::copyExternalImageToTexture);
    installMethodWithRuntime(runtime, prototype, "drawElementImageToTexture",
                             &GPUQueue::drawElementImageToTexture);
    installGetterSetter(runtime, prototype, "label", &GPUQueue::getLabel,
                        &GPUQueue::setLabel);
  }

  inline const wgpu::Queue get() { return _instance; }

private:
  wgpu::Queue _instance;
  std::shared_ptr<async::RuntimeContext> _async;
  std::string _label;
};

} // namespace rnwgpu
