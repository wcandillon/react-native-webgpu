#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUSamplerDescriptor {
public:
  wgpu::SamplerDescriptor *getInstance() { return &_instance; }

private:
  wgpu::SamplerDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUSamplerDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUSamplerDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUSamplerDescriptor>();
    if (value.hasProperty(runtime, "addressModeU")) {
      auto addressModeU = value.getProperty(runtime, "addressModeU");
      if (addressModeU.isNumber()) {
        result->_instance.addressModeU = addressModeU.getNumber();
      } else if (addressModeU.isNull() || addressModeU.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::addressModeU is required");
      }
    }
    if (value.hasProperty(runtime, "addressModeV")) {
      auto addressModeV = value.getProperty(runtime, "addressModeV");
      if (addressModeV.isNumber()) {
        result->_instance.addressModeV = addressModeV.getNumber();
      } else if (addressModeV.isNull() || addressModeV.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::addressModeV is required");
      }
    }
    if (value.hasProperty(runtime, "addressModeW")) {
      auto addressModeW = value.getProperty(runtime, "addressModeW");
      if (addressModeW.isNumber()) {
        result->_instance.addressModeW = addressModeW.getNumber();
      } else if (addressModeW.isNull() || addressModeW.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::addressModeW is required");
      }
    }
    if (value.hasProperty(runtime, "magFilter")) {
      auto magFilter = value.getProperty(runtime, "magFilter");
      if (magFilter.isNumber()) {
        result->_instance.magFilter = magFilter.getNumber();
      } else if (magFilter.isNull() || magFilter.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::magFilter is required");
      }
    }
    if (value.hasProperty(runtime, "minFilter")) {
      auto minFilter = value.getProperty(runtime, "minFilter");
      if (minFilter.isNumber()) {
        result->_instance.minFilter = minFilter.getNumber();
      } else if (minFilter.isNull() || minFilter.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::minFilter is required");
      }
    }
    if (value.hasProperty(runtime, "mipmapFilter")) {
      auto mipmapFilter = value.getProperty(runtime, "mipmapFilter");
      if (mipmapFilter.isNumber()) {
        result->_instance.mipmapFilter = mipmapFilter.getNumber();
      } else if (mipmapFilter.isNull() || mipmapFilter.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::mipmapFilter is required");
      }
    }
    if (value.hasProperty(runtime, "lodMinClamp")) {
      auto lodMinClamp = value.getProperty(runtime, "lodMinClamp");
      if (lodMinClamp.isNumber()) {
        result->_instance.lodMinClamp = lodMinClamp.getNumber();
      } else if (lodMinClamp.isNull() || lodMinClamp.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::lodMinClamp is required");
      }
    }
    if (value.hasProperty(runtime, "lodMaxClamp")) {
      auto lodMaxClamp = value.getProperty(runtime, "lodMaxClamp");
      if (lodMaxClamp.isNumber()) {
        result->_instance.lodMaxClamp = lodMaxClamp.getNumber();
      } else if (lodMaxClamp.isNull() || lodMaxClamp.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::lodMaxClamp is required");
      }
    }
    if (value.hasProperty(runtime, "compare")) {
      auto compare = value.getProperty(runtime, "compare");
      if (compare.isNumber()) {
        result->_instance.compare = compare.getNumber();
      } else if (compare.isNull() || compare.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::compare is required");
      }
    }
    if (value.hasProperty(runtime, "maxAnisotropy")) {
      auto maxAnisotropy = value.getProperty(runtime, "maxAnisotropy");
      if (maxAnisotropy.isNumber()) {
        result->_instance.maxAnisotropy = maxAnisotropy.getNumber();
      } else if (maxAnisotropy.isNull() || maxAnisotropy.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerDescriptor::maxAnisotropy is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUSamplerDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
