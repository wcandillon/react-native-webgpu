#include "GPUQueue.h"

#include <limits>

namespace rnwgpu {

void GPUQueue::submit(
    std::vector<std::shared_ptr<GPUCommandBuffer>> commandBuffers) {
  auto bufs = conv(commandBuffers);
  _instance.Submit(bufs.size(), bufs.data());
}

void GPUQueue::writeBuffer(std::shared_ptr<GPUBuffer> buffer,
                           uint64_t bufferOffset,
                           std::shared_ptr<ArrayBuffer> src,
                           std::optional<uint64_t> dataOffsetElements,
                           std::optional<size_t> sizeElements) {
  auto buf = buffer->get();
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
  size_t size = src->_size;
  if (sizeElements.has_value()) {
    if (sizeElements.value() >
        std::numeric_limits<uint64_t>::max() / src->_bytesPerElement) {
      throw std::runtime_error("size overflows.");
      return;
    }
    size = sizeElements.value() * src->_bytesPerElement;
  }

  if (size > src->_size) {
    throw std::runtime_error("size is larger than data's size.");
  }

  if (size % 4 != 0) {
    throw std::runtime_error("size is not a multiple of 4 bytes.");
  }

  _instance.WriteBuffer(buf, bufferOffset, src->_data, size);
}

} // namespace rnwgpu
