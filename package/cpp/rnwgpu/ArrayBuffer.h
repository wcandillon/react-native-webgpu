#pragma once
#include <jsi/jsi.h>

#include <memory>

#include "RNFJSIConverter.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

struct ArrayBuffer : jsi::MutableBuffer {
  ArrayBuffer(void *data, size_t size, size_t bytesPerElement = 4)
      : _data(data), _size(size), _bytesPerElement(bytesPerElement) {}

  ~ArrayBuffer() override {}

  // Implement the size() method
  virtual size_t size() const override { return _size; }

  // Implement the data() method
  uint8_t *data() override { return static_cast<uint8_t *>(_data); }

  void *_data;
  size_t _size;
  size_t _bytesPerElement;
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::ArrayBuffer>> {
  static std::shared_ptr<rnwgpu::ArrayBuffer>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBound) {
    if (!arg.getObject(runtime).isArrayBuffer(runtime)) {
      throw std::runtime_error(
          "Buffer::fromJSI: argument is not an ArrayBuffer");
    }
    auto data = arg.getObject(runtime).getArrayBuffer(runtime);
    auto length = data.length(runtime);
    auto size = data.size(runtime);
    auto bytesPerElement = size / length;
    auto result =
        std::make_shared<rnwgpu::ArrayBuffer>(data.data(runtime), size);
    return result;
  }

  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::ArrayBuffer> arg) {
    auto val = jsi::ArrayBuffer(runtime, arg);
    auto d = val.data(runtime);
    return val;
  }
};

} // namespace margelo
