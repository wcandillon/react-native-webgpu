#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "RNFHybridObject.h"

namespace rnwgpu {

class ImageData2: public margelo::HybridObject {
public:
  ImageData2() : HybridObject("ImageData") {}
  void* data;
  size_t size;
  size_t width;
  size_t height;
  wgpu::TextureFormat format;
    
    void loadHybridMethods() override {
        
    }
};

} // namespace rnwgpu

