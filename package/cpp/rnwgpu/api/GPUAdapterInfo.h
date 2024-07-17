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

class GPUAdapterInfo : public m::HybridObject {
public:
    explicit GPUAdapterInfo(wgpu::AdapterInfo instance) : HybridObject("GPUAdapterInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }


  

  

  

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapterInfo::getBrand, this);
    
    
    
  }
  
  inline const wgpu::AdapterInfo get() {
    return _instance;
  }

 private:
  wgpu::AdapterInfo _instance;
  
};
} // namespace rnwgpu