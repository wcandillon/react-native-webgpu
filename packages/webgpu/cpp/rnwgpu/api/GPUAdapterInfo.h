#pragma once

#include <string>
#include <utility>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"
#include "Convertors.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUAdapterInfo : public m::HybridObject {
public:
  explicit GPUAdapterInfo(wgpu::AdapterInfo &info)
      : HybridObject("GPUAdapterInfo"), _vendor(info.vendor),
        _architecture(info.architecture), _device(info.device),
        _description(info.description) {}

public:
  std::string getBrand() { return _name; }

  std::string getVendor() { return _vendor; }
  std::string getArchitecture() { return _architecture; }
  std::string getDevice() { return _device; }
  std::string getDescription() { return _description; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapterInfo::getBrand, this);

    registerHybridGetter("vendor", &GPUAdapterInfo::getVendor, this);
    registerHybridGetter("architecture", &GPUAdapterInfo::getArchitecture,
                         this);
    registerHybridGetter("device", &GPUAdapterInfo::getDevice, this);
    registerHybridGetter("description", &GPUAdapterInfo::getDescription, this);
  }

private:
  std::string _vendor;
  std::string _architecture;
  std::string _device;
  std::string _description;
};

} // namespace rnwgpu
