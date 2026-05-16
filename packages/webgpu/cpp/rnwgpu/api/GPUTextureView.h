#pragma once

#include <memory>
#include <string>
#include <utility>

#include "Unions.h"

#include "NativeObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class SurfaceInfo;

class GPUTextureView : public NativeObject<GPUTextureView> {
public:
  static constexpr const char *CLASS_NAME = "GPUTextureView";

  explicit GPUTextureView(wgpu::TextureView instance, std::string label,
                          std::weak_ptr<SurfaceInfo> surfaceInfo = {})
      : NativeObject(CLASS_NAME), _instance(instance), _label(label),
        _surfaceInfo(std::move(surfaceInfo)) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUTextureView::getBrand);
    installGetterSetter(runtime, prototype, "label", &GPUTextureView::getLabel,
                        &GPUTextureView::setLabel);
  }

  inline const wgpu::TextureView get() { return _instance; }
  std::weak_ptr<SurfaceInfo> getSurfaceInfo() { return _surfaceInfo; }

private:
  wgpu::TextureView _instance;
  std::string _label;
  std::weak_ptr<SurfaceInfo> _surfaceInfo;
};

} // namespace rnwgpu
