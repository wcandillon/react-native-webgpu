#include "GPUBuffer.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

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
  std::stringstream ss;
  uint8_t *uint8_ptr = static_cast<uint8_t *>(ptr);
  ss << "Buffer content: ";
  for (size_t i = 0; i < s; ++i) {
    ss << std::setw(2) << std::setfill('0') << std::hex
       << static_cast<int>(uint8_ptr[i]) << " ";
  }
  std::string bufferContent = ss.str();
  Logger::logToConsole("debug: %s", bufferContent.c_str());
  // TODO(crbug.com/dawn/1135): Ownership here is the wrong way around.
  // mappings_.emplace_back(Mapping{start, end,
  // Napi::Persistent(array_buffer)});
  return array_buffer;
}

void GPUBuffer::destroy() { _instance.Destroy(); }

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
