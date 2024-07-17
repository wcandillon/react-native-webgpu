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

class GPUCompilationMessage : public m::HybridObject {
public:
    explicit GPUCompilationMessage(wgpu::CompilationMessage instance) : HybridObject("GPUCompilationMessage"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }


  

  

  

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCompilationMessage::getBrand, this);
    
    
    
  }
  
  inline const wgpu::CompilationMessage get() {
    return _instance;
  }

 private:
  wgpu::CompilationMessage _instance;
  
};
} // namespace rnwgpu