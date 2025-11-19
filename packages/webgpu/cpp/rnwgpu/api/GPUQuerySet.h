#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUQuerySet : public m::HybridObject {
public:
  explicit GPUQuerySet(wgpu::QuerySet instance, std::string label)
      : HybridObject("GPUQuerySet"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  void destroy();

  wgpu::QueryType getType();
  uint32_t getCount();

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQuerySet::getBrand, this);
    registerHybridMethod("destroy", &GPUQuerySet::destroy, this);
    registerHybridGetter("type", &GPUQuerySet::getType, this);
    registerHybridGetter("count", &GPUQuerySet::getCount, this);
    registerHybridGetter("label", &GPUQuerySet::getLabel, this);
    registerHybridSetter("label", &GPUQuerySet::setLabel, this);
  }

  inline const wgpu::QuerySet get() { return _instance; }

  size_t getMemoryPressure() override {
    const uint32_t count = getCount();
    const wgpu::QueryType type = getType();

    size_t bytesPerQuery = 16; // default to an overshoot
    switch (type) {
    case wgpu::QueryType::Occlusion:
      bytesPerQuery = 16; // occlusion result is 64-bit; pad to 16 for safety
      break;
    case wgpu::QueryType::Timestamp:
      bytesPerQuery = 16; // timestamps are 64-bit; double to overshoot
      break;
    case wgpu::QueryType::PipelineStatistics: {
      constexpr size_t kAssumedCountersPerQuery = 8;
      bytesPerQuery = 8 * kAssumedCountersPerQuery;
      break;
    }
    default:
      break;
    }

    return static_cast<size_t>(count) * bytesPerQuery;
  }

private:
  wgpu::QuerySet _instance;
  std::string _label;
};

} // namespace rnwgpu
