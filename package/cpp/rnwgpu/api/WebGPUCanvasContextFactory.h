#pragma once

#include <memory>
#include <string>
#include <utility>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "GPUCanvasContext.h"

#ifdef __APPLE__
#include "WebGPUModule.h"
#endif

namespace rnwgpu {

namespace m = margelo;

class WebGPUCanvasContextFactory : public m::HybridObject {
public:
  GPUCanvasContextFactory() : HybridObject("GPUCanvasContextFactory") {}

public:
  std::shared_ptr<GPUCanvasContext> Make(uint64_t surface, int width,
                                         int height) {
#ifdef __APPLE__
    wgpu::SurfaceDescriptorFromMetalLayer metalSurfaceDesc;
    metalSurfaceDesc.layer = reinterpret_cast<void *>(surface);

    wgpu::SurfaceDescriptor surfaceDescriptor;
    surfaceDescriptor.nextInChain = &metalSurfaceDesc;

    auto surfaceGpu = std::make_unique<wgpu::Surface>(
        WebGPUModule::getManager()->getGPU()->get().CreateSurface(
            &surfaceDescriptor));
    const float scaleFactor = 1;
    const float scaledWidth = width * scaleFactor;
    const float scaledHeight = height * scaleFactor;

    rnwgpu::SurfaceData surfaceData{scaledWidth, scaledHeight,
                                    std::move(surfaceGpu)};
#elif __ANDROID__
    throw std::runtime_error("Not implemented");
#endif
    return std::make_shared<rnwgpu::GPUCanvasContext>(surfaceData);
  }

  void loadHybridMethods() override {
    registerHybridGetter("Make", &GPUCanvasContextFactory::Make, this);
  }
};

} // namespace rnwgpu
