#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Unions.h"

#include "NativeObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class SurfaceInfo;

class GPUCommandBuffer : public NativeObject<GPUCommandBuffer> {
public:
  static constexpr const char *CLASS_NAME = "GPUCommandBuffer";

  explicit GPUCommandBuffer(
      wgpu::CommandBuffer instance, std::string label,
      std::vector<std::weak_ptr<SurfaceInfo>> presentableSurfaces = {})
      : NativeObject(CLASS_NAME), _instance(instance), _label(label),
        _presentableSurfaces(std::move(presentableSurfaces)) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUCommandBuffer::getBrand);
    installGetterSetter(runtime, prototype, "label",
                        &GPUCommandBuffer::getLabel,
                        &GPUCommandBuffer::setLabel);
  }

  inline const wgpu::CommandBuffer get() { return _instance; }
  const std::vector<std::weak_ptr<SurfaceInfo>> &getPresentableSurfaces() {
    return _presentableSurfaces;
  }

private:
  wgpu::CommandBuffer _instance;
  std::string _label;
  std::vector<std::weak_ptr<SurfaceInfo>> _presentableSurfaces;
};

} // namespace rnwgpu
