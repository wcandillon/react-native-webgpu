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

class GPUPipelineLayout : public m::HybridObject {
public:
    explicit GPUPipelineLayout(wgpu::PipelineLayout instance, std::string label) : HybridObject("GPUPipelineLayout"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }


  

  

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUPipelineLayout::getBrand, this);
    
    
    registerHybridGetter("label", &GPUPipelineLayout::getLabel, this);
  }
  
  inline const wgpu::PipelineLayout get() {
    return _instance;
  }

 private:
  wgpu::PipelineLayout _instance;
std::string _label;
  
};
} // namespace rnwgpu