#include "GPUQueue.h"

namespace rnwgpu {

void GPUQueue::submit(
    std::vector<std::shared_ptr<GPUCommandBuffer>> commandBuffers) {
  std::vector<wgpu::CommandBuffer> bufs(commandBuffers.size());
  for (size_t i = 0; i < commandBuffers.size(); i++) {
    bufs[i] = commandBuffers[i]->_instance;
  }
  _instance.Submit(bufs.size(), bufs.data());
}

void GPUQueue::writeBuffer(std::shared_ptr<GPUBuffer> buffer,
                           uint64_t bufferOffset,
                           std::shared_ptr<ArrayBuffer> src,
                           std::optional<uint64_t> dataOffsetElements,
                           std::optional<size_t> sizeElements) {
  auto buf = buffer->_instance;
  // Note that in the JS semantics of WebGPU, writeBuffer works in number of
  // elements of the typed arrays.
  if (dataOffsetElements > uint64_t(src->_size / src->_bytesPerElement)) {
    throw std::runtime_error("dataOffset is larger than data's size.");
    return;
  }
  uint64_t dataOffset = dataOffsetElements.value_or(0) * src->_bytesPerElement;
  src->_data = reinterpret_cast<uint8_t *>(src->_data) + dataOffset;
  src->_size -= dataOffset;

  // Size defaults to dataSize - dataOffset. Instead of computing in elements,
  // we directly use it in bytes, and convert the provided value, if any, in
  // bytes.
  uint64_t size64 = uint64_t(src->_size);
  if (sizeElements.has_value()) {
    if (sizeElements.value() >
        std::numeric_limits<uint64_t>::max() / src->_bytesPerElement) {
      throw std::runtime_error("size overflows.");
      return;
    }
    size64 = sizeElements.value() * src->_bytesPerElement;
  }

  if (size64 > uint64_t(src->_size)) {
    throw std::runtime_error("size is larger than data's size.");
  }

  if (size64 % 4 != 0) {
    throw std::runtime_error("size is not a multiple of 4 bytes.");
  }

  assert(size64 <= std::numeric_limits<size_t>::max());
  _instance.WriteBuffer(buf, bufferOffset, src->_data,
                        static_cast<size_t>(size64));
}

} // namespace rnwgpu
