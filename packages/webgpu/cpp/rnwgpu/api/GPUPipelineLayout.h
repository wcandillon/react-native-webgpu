#pragma once

#include <string>

#include "Unions.h"

#include "RNFNativeObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;
namespace jsi = facebook::jsi;

class GPUPipelineLayout : public m::NativeObject<GPUPipelineLayout> {
public:
  static constexpr const char *CLASS_NAME = "GPUPipelineLayout";

  explicit GPUPipelineLayout(wgpu::PipelineLayout instance, std::string label)
      : NativeObject(CLASS_NAME), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUPipelineLayout::getBrand);
    installGetterSetter(runtime, prototype, "label",
                        &GPUPipelineLayout::getLabel,
                        &GPUPipelineLayout::setLabel);
  }

  inline const wgpu::PipelineLayout get() { return _instance; }

private:
  wgpu::PipelineLayout _instance;
  std::string _label;
};

} // namespace rnwgpu
