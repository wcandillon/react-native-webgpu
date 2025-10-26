#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunnerLegacy.h"

#include "webgpu/webgpu_cpp.h"

#include "ArrayBuffer.h"
#include "GPUBuffer.h"
#include "GPUCommandBuffer.h"
#include "GPUImageCopyExternalImage.h"
#include "GPUImageCopyTextureTagged.h"

namespace rnwgpu {

namespace m = margelo;

class GPUQueue : public m::HybridObject {
public:
  explicit GPUQueue(wgpu::Queue instance,
                    std::shared_ptr<AsyncRunnerLegacy> async, std::string label)
      : HybridObject("GPUQueue"), _instance(instance), _async(async),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  void submit(std::vector<std::shared_ptr<GPUCommandBuffer>> commandBuffers);
  std::future<void> onSubmittedWorkDone();
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

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQueue::getBrand, this);
    registerHybridMethod("submit", &GPUQueue::submit, this);
    registerHybridMethod("onSubmittedWorkDone", &GPUQueue::onSubmittedWorkDone,
                         this);
    registerHybridMethod("writeBuffer", &GPUQueue::writeBuffer, this);
    registerHybridMethod("writeTexture", &GPUQueue::writeTexture, this);
    registerHybridMethod("copyExternalImageToTexture",
                         &GPUQueue::copyExternalImageToTexture, this);

    registerHybridGetter("label", &GPUQueue::getLabel, this);
    registerHybridSetter("label", &GPUQueue::setLabel, this);
  }

  inline const wgpu::Queue get() { return _instance; }

private:
  wgpu::Queue _instance;
  std::shared_ptr<AsyncRunnerLegacy> _async;
  std::string _label;
};

} // namespace rnwgpu