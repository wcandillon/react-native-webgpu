#include "GPUBuffer.h"

namespace rnwgpu {

std::shared_ptr<MutableJSIBuffer>
GPUBuffer::getMappedRange(std::optional<size_t> o, std::optional<size_t> size) {
  size_t offset = o.value_or(0);
  auto bufferSize = _instance.GetSize();
  uint64_t s = size.has_value() ? size.value() : (bufferSize - offset);
  uint64_t start = offset;
  uint64_t end = offset + s;
  for (auto &mapping : mappings_) {
    if (mapping.Intersects(start, end)) {
      throw std::runtime_error("Buffer is already mapped");
    }
  }
  auto usage = _instance.GetUsage();
  auto *ptr =
      (usage & wgpu::BufferUsage::MapWrite)
          ? _instance.GetMappedRange(offset, s)
          : const_cast<void *>(_instance.GetConstMappedRange(offset, s));
  if (!ptr) {
    throw std::runtime_error("Failed to map buffer");
  }
  auto buffer = std::make_shared<MutableJSIBuffer>(ptr, s);
  mappings_.emplace_back(Mapping{start, end, buffer});
  return buffer;
}

std::future<void> GPUBuffer::mapAsync(size_t mode, std::optional<size_t> offset,
                                      std::optional<size_t> size) {
  std::promise<void> promise;
  std::future<void> result = promise.get_future();

  wgpu::BufferMapCallbackInfo callbackInfo = {
    nullptr, wgpu::CallbackMode::AllowSpontaneous,
    [](WGPUBufferMapAsyncStatus status, void *userdata) {
      auto pPromise = static_cast<std::promise<void> *>(userdata);
      if (status == WGPUBufferMapAsyncStatus_Success) {
        pPromise->set_value();
      } else {
        pPromise->set_exception(std::make_exception_ptr(
            std::runtime_error("Buffer mapping failed")));
      }
      delete pPromise;
    },
    new std::promise<void>(std::move(promise))
  };
  wgpu::Future future = _instance.MapAsync(static_cast<wgpu::MapMode>(mode), offset.value_or(0), size.value_or(_instance.GetSize()), callbackInfo);
  wgpu::FutureWaitInfo waitInfo = {future};

  //_instance.WaitAny(1, &waitInfo, UINT64_MAX);
  return result;
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
