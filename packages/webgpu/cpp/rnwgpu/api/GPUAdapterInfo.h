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
  explicit GPUAdapterInfo(wgpu::AdapterInfo &instance)
      : HybridObject("GPUAdapterInfo"), _instance(std::move(instance)) {}

public:
  std::string getBrand() { return _name; }

  std::string getVendor() { return _instance.vendor.data; }
  std::string getArchitecture() {
    return _instance.architecture.data;
  }
  std::string getDevice() { return _instance.device.data; }
  std::string getDescription() {
    return _instance.description.data;
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapterInfo::getBrand, this);

    registerHybridGetter("vendor", &GPUAdapterInfo::getVendor, this);
    registerHybridGetter("architecture", &GPUAdapterInfo::getArchitecture,
                         this);
    registerHybridGetter("device", &GPUAdapterInfo::getDevice, this);
    registerHybridGetter("description", &GPUAdapterInfo::getDescription, this);
  }

private:
  wgpu::AdapterInfo _instance;
};

} // namespace rnwgpu
