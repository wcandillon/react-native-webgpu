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

class GPUDeviceLostInfo : public m::HybridObject {
public:
    explicit GPUDeviceLostInfo(wgpu::DeviceLostInfo instance) : HybridObject("GPUDeviceLostInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }


  

  

  

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDeviceLostInfo::getBrand, this);
    
    
    
  }
  
  inline const wgpu::DeviceLostInfo get() {
    return _instance;
  }

 private:
  wgpu::DeviceLostInfo _instance;
  
};
} // namespace rnwgpu