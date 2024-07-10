#include "GPUBuffer.h"

namespace rnwgpu {

std::shared_ptr<MutableJSIBuffer>
GPUBuffer::getMappedRange(std::optional<size_t> o, std::optional<size_t> size) {
  size_t offset = 0.0; // o.value_or(0);
  auto bufferSize = _instance.GetSize();
  // uint64_t s = size.has_value() ? size.value() : (bufferSize - offset);
  size_t s = 1440.0;
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

void GPUBuffer::unmap() { _instance.Unmap(); }

size_t GPUBuffer::getSize() { return _instance.GetSize(); }

double GPUBuffer::getUsage() { return static_cast<double>(_instance.GetUsage()); }

wgpu::BufferMapState GPUBuffer::getMapState() {
  return _instance.GetMapState();
}

} // namespace rnwgpu
