#pragma once
#include <jsi/jsi.h>

#include <memory>

namespace rnwgpu {

namespace jsi = facebook::jsi;

struct MutableJSIBuffer : jsi::MutableBuffer {
  MutableJSIBuffer(void *data, size_t size) : _data(data), _size(size) {}

  size_t size() const override { return _size; }

  uint8_t *data() override { return reinterpret_cast<uint8_t *>(_data); }

  void *_data;
  size_t _size;
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::MutableJSIBuffer>> {
  static std::shared_ptr<rnwgpu::MutableJSIBuffer>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    if (!arg.getObject(runtime).isArrayBuffer(runtime)) {
      throw std::runtime_error(
          "Buffer::fromJSI: argument is not an ArrayBuffer");
    }
    auto data = arg.getObject(runtime).getArrayBuffer(runtime);
    auto result = std::make_unique<rnwgpu::MutableJSIBuffer>(
        data.data(runtime), data.size(runtime));
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::MutableJSIBuffer> arg) {
    auto val = jsi::ArrayBuffer(runtime, arg);
    auto d = val.data(runtime);
    return val;
  }
};

} // namespace margelo
