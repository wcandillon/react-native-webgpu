#include "GPUBuffer.h"

#include <utility>

namespace rnwgpu {

std::shared_ptr<ArrayBuffer>
GPUBuffer::getMappedRange(std::optional<size_t> o, std::optional<size_t> size) {
  size_t offset = o.value_or(0);
  auto bufferSize = _instance.GetSize();
  uint64_t s = size.has_value() ? size.value() : (bufferSize - offset);
  uint64_t start = offset;
  uint64_t end = offset + s;
  // for (auto &mapping : mappings) {
  //   if (mapping.Intersects(start, end)) {
  //     throw std::runtime_error("Buffer is already mapped");
  //   }
  // }
  auto usage = _instance.GetUsage();
  auto *ptr =
      (usage & wgpu::BufferUsage::MapWrite)
          ? _instance.GetMappedRange(offset, s)
          : const_cast<void *>(_instance.GetConstMappedRange(offset, s));
  if (!ptr) {
    throw std::runtime_error("Failed to map buffer");
  }
  auto buffer = std::make_shared<ArrayBuffer>(ptr, s);
  // mappings.emplace_back(Mapping{start, end, buffer});
  return buffer;
}

std::future<void> GPUBuffer::mapAsync(uint64_t mode, std::optional<size_t> o,
                                      std::optional<size_t> size) {
  return _async->runAsync([=] {
    auto md = static_cast<wgpu::MapMode>(mode);
    uint64_t offset = o.value_or(0);
    uint64_t s =
        size.has_value() ? size.value() : (_instance.GetSize() - offset);
    uint64_t start = offset;
    uint64_t end = offset + s;

    // for (auto& mapping : mappings) {
    //   if (mapping.Intersects(start, end)) {
    //     promise.set_exception(std::make_exception_ptr(std::runtime_error("Buffer
    //     is already mapped"))); return future;
    //   }
    // }
    wgpu::BufferMapCallbackInfo callback;
    callback.mode = wgpu::CallbackMode::WaitAnyOnly;
    callback.callback = [](WGPUBufferMapAsyncStatus status, void *userdata) {
      switch (status) {
      case WGPUBufferMapAsyncStatus_Success:
        break;
      case WGPUBufferMapAsyncStatus_InstanceDropped:
        throw std::runtime_error("WGPUBufferMapAsyncStatus_InstanceDropped");
        break;
      case WGPUBufferMapAsyncStatus_ValidationError:
        throw std::runtime_error("WGPUBufferMapAsyncStatus_ValidationError");
        break;
      default:
        throw std::runtime_error("WGPUBufferMapAsyncStatus: " +
                                 std::to_string(status));
        break;
      }
    };
    return _instance.MapAsync(md, offset, s, callback);
  });
}

void GPUBuffer::unmap() { _instance.Unmap(); }

size_t GPUBuffer::getSize() { return _instance.GetSize(); }

double GPUBuffer::getUsage() {
  return static_cast<double>(_instance.GetUsage());
}

wgpu::BufferMapState GPUBuffer::getMapState() {
  return _instance.GetMapState();
}

} // namespace rnwgpu
