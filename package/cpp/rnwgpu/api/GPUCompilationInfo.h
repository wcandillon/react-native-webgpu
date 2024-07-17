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

class GPUCompilationInfo : public m::HybridObject {
public:
    explicit GPUCompilationInfo(wgpu::CompilationInfo instance) : HybridObject("GPUCompilationInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }


  

  

  

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCompilationInfo::getBrand, this);
    
    
    
  }
  
  inline const wgpu::CompilationInfo get() {
    return _instance;
  }

 private:
  wgpu::CompilationInfo _instance;
  
};
} // namespace rnwgpu