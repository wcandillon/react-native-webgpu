#include "GPUBuffer.h"

#include <utility>

#include "Convertors.h"

namespace rnwgpu {

std::shared_ptr<ArrayBuffer>
GPUBuffer::getMappedRange(std::optional<size_t> o, std::optional<size_t> size) {
  auto offset = o.value_or(0);
  uint64_t s = size.has_value() ? size.value() : (_instance.GetSize() - offset);

  uint64_t start = offset;
  uint64_t end = offset + s;
  // for (auto& mapping : mappings_) {
  //     if (mapping.Intersects(start, end)) {
  //         Errors::OperationError(env).ThrowAsJavaScriptException();
  //         return {};
  //     }
  // }

  auto *ptr =
      (_instance.GetUsage() & wgpu::BufferUsage::MapWrite)
          ? _instance.GetMappedRange(offset, s)
          : const_cast<void *>(_instance.GetConstMappedRange(offset, s));
  if (!ptr) {
    throw std::runtime_error("Failed to get getMappedRange");
  }
  auto array_buffer = std::make_shared<ArrayBuffer>(ptr, s, 1);
  // TODO(crbug.com/dawn/1135): Ownership here is the wrong way around.
  // mappings_.emplace_back(Mapping{start, end,
  // Napi::Persistent(array_buffer)});
  return array_buffer;
}

void GPUBuffer::destroy() { _instance.Destroy(); }

std::future<void> GPUBuffer::mapAsync(uint64_t modeIn,
                                      std::optional<uint64_t> offset,
                                      std::optional<uint64_t> size) {
  Convertor conv;
  wgpu::MapMode mode;
  if (!conv(mode, modeIn)) {
    throw std::runtime_error("Couldn't get MapMode");
  }
  uint64_t rangeSize = size.has_value()
                           ? size.value()
                           : (_instance.GetSize() - offset.value_or(0));
  return _async->runAsync([=] {
    // for (auto& mapping : mappings) {
    //   if (mapping.Intersects(start, end)) {
    //     promise.set_exception(std::make_exception_ptr(std::runtime_error("Buffer
    //     is already mapped"))); return future;
    //   }
    // }
    return _instance.MapAsync(
        mode, offset.value_or(0), rangeSize, wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::MapAsyncStatus status, wgpu::StringView message) {
          switch (status) {
          case wgpu::MapAsyncStatus::Success:
            break;
          case wgpu::MapAsyncStatus::CallbackCancelled:
            throw std::runtime_error("MapAsyncStatus::CallbackCancelled");
            break;
          case wgpu::MapAsyncStatus::Error:
            throw std::runtime_error("MapAsyncStatus::Error");
            break;
          case wgpu::MapAsyncStatus::Aborted:
            throw std::runtime_error("MapAsyncStatus::Aborted");
            break;
          default:
            throw std::runtime_error("MapAsyncStatus: " +
                                     std::to_string(static_cast<int>(status)));
            break;
          }
        });
  });
}

void GPUBuffer::unmap() { _instance.Unmap(); }

uint64_t GPUBuffer::getSize() { return _instance.GetSize(); }

double GPUBuffer::getUsage() {
  return static_cast<double>(_instance.GetUsage());
}

wgpu::BufferMapState GPUBuffer::getMapState() {
  return _instance.GetMapState();
}

} // namespace rnwgpu
