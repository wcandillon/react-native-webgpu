#pragma once

#include <algorithm>
#include <memory>
#include <string>

#include "Unions.h"

#include "NativeObject.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUTextureView.h"
#include "GPUTextureViewDescriptor.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUTexture : public NativeObject<GPUTexture> {
public:
  static constexpr const char *CLASS_NAME = "GPUTexture";

  explicit GPUTexture(wgpu::Texture instance, std::string label,
                      bool ownsMemory = true)
      : NativeObject(CLASS_NAME), _instance(instance), _label(label),
        _ownsMemory(ownsMemory) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  std::shared_ptr<GPUTextureView> createView(
      std::optional<std::shared_ptr<GPUTextureViewDescriptor>> descriptor);
  void destroy();

  uint32_t getWidth();
  uint32_t getHeight();
  uint32_t getDepthOrArrayLayers();
  uint32_t getMipLevelCount();
  uint32_t getSampleCount();
  wgpu::TextureDimension getDimension();
  wgpu::TextureFormat getFormat();
  double getUsage();

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUTexture::getBrand);
    installMethod(runtime, prototype, "createView", &GPUTexture::createView);
    installMethod(runtime, prototype, "destroy", &GPUTexture::destroy);
    installGetter(runtime, prototype, "width", &GPUTexture::getWidth);
    installGetter(runtime, prototype, "height", &GPUTexture::getHeight);
    installGetter(runtime, prototype, "depthOrArrayLayers",
                  &GPUTexture::getDepthOrArrayLayers);
    installGetter(runtime, prototype, "mipLevelCount",
                  &GPUTexture::getMipLevelCount);
    installGetter(runtime, prototype, "sampleCount",
                  &GPUTexture::getSampleCount);
    installGetter(runtime, prototype, "dimension", &GPUTexture::getDimension);
    installGetter(runtime, prototype, "format", &GPUTexture::getFormat);
    installGetter(runtime, prototype, "usage", &GPUTexture::getUsage);
    installGetterSetter(runtime, prototype, "label", &GPUTexture::getLabel,
                        &GPUTexture::setLabel);
  }

  inline const wgpu::Texture get() { return _instance; }

  size_t getMemoryPressure() override {
    return _ownsMemory ? _computeMemoryPressure() : sizeof(GPUTexture);
  }

private:
  size_t _computeMemoryPressure();

  wgpu::Texture _instance;
  std::string _label;
  bool _ownsMemory = true;
};

} // namespace rnwgpu
