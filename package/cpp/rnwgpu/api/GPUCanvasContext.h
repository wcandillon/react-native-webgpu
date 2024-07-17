#pragma once

#include <memory>
#include <string>
#include <future>
#include <vector>

#include "Unions.h"
#include "Convertors.h"
#include <RNFHybridObject.h>

#include "AsyncRunner.h"
#include "ArrayBuffer.h"

#include "webgpu/webgpu_cpp.h"



namespace rnwgpu {

namespace m = margelo;

class GPUCanvasContext : public m::HybridObject {
public:
    explicit GPUCanvasContext(wgpu::CanvasContext instance) : HybridObject("GPUCanvasContext"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }


  

  

  

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCanvasContext::getBrand, this);
    
    
    
  }
  
  inline const wgpu::CanvasContext get() {
    return _instance;
  }

 private:
  wgpu::CanvasContext _instance;
  
};
} // namespace rnwgpu