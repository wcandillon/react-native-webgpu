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
  size_t size() const override { return _size; }

  // Implement the data() method
  uint8_t *data() override { return static_cast<uint8_t *>(_data); }

  void *_data;
  size_t _size;
  size_t _bytesPerElement;
};

} // namespace rnwgpu

namespace margelo {

static std::shared_ptr<rnwgpu::ArrayBuffer>
createArrayBufferFromJSI(jsi::Runtime &runtime, const jsi::ArrayBuffer &arrayBuffer) {
  auto length = arrayBuffer.length(runtime);
  auto size = arrayBuffer.size(runtime);
  auto bytesPerElement = size / length;
  return std::make_shared<rnwgpu::ArrayBuffer>(
      arrayBuffer.data(runtime), size, bytesPerElement);
}

template <> struct JSIConverter<std::shared_ptr<rnwgpu::ArrayBuffer>> {
static std::shared_ptr<rnwgpu::ArrayBuffer>
fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBound) {
  if (arg.isObject()) {
    auto obj = arg.getObject(runtime);
    if (obj.isArrayBuffer(runtime)) {
      return createArrayBufferFromJSI(runtime, obj.getArrayBuffer(runtime));
    }
    if (obj.hasProperty(runtime, "buffer")) {
      auto bufferProp = obj.getProperty(runtime, "buffer");
      if (bufferProp.isObject() &&
          bufferProp.getObject(runtime).isArrayBuffer(runtime)) {
        return createArrayBufferFromJSI(runtime, bufferProp.getObject(runtime).getArrayBuffer(runtime));
      }
    }
  }
  throw std::runtime_error("ArrayBuffer::fromJSI: argument is not an object "
                           "with an ArrayBuffer 'buffer' property");
}

  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::ArrayBuffer> arg) {
    return jsi::ArrayBuffer(runtime, arg);
  }
};

} // namespace margelo
