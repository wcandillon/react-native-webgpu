#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDeviceLostInfo : public m::HybridObject {
public:
  explicit GPUDeviceLostInfo(wgpu::DeviceLostReason reason, std::string message)
      : HybridObject("GPUDeviceLostInfo"), _reason(reason), _message(message) {}

public:
  std::string getBrand() { return _name; }

  wgpu::DeviceLostReason getReason() { return _reason; }
  std::string getMessage() { return _message; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDeviceLostInfo::getBrand, this);

    registerHybridGetter("reason", &GPUDeviceLostInfo::getReason, this);
    registerHybridGetter("message", &GPUDeviceLostInfo::getMessage, this);
  }

  size_t getMemoryPressure() override {
    return sizeof(wgpu::DeviceLostReason) + _message.capacity();
  }

private:
  wgpu::DeviceLostReason _reason;
  std::string _message;
};

} // namespace rnwgpu
