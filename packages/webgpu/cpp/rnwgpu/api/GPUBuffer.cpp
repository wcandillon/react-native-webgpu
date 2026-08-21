#include "GPUBuffer.h"

#include <cstring>
#include <memory>
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

namespace {
// readSync returns a copy that outlives the mapping, so it needs an
// ArrayBuffer that owns (and frees) its backing store - the base class
// wraps foreign memory and never frees.
struct OwnedArrayBuffer : ArrayBuffer {
  explicit OwnedArrayBuffer(size_t size)
      : ArrayBuffer(malloc(size), size, 1) {}
  ~OwnedArrayBuffer() override { free(_data); }
};
} // namespace

std::shared_ptr<ArrayBuffer>
GPUBuffer::readSync(std::optional<size_t> o, std::optional<size_t> sizeIn) {
  // Synchronous small-buffer readback: blocks the calling thread until all
  // previously submitted GPU work using this buffer completes, then returns
  // a copy of the mapped bytes. Built for tiny compute results (landmarks,
  // ranges, counters) that must be consumed in the SAME frame - the async
  // mapAsync path forces at least one frame of staleness in render loops
  // that cannot await. Requires MAP_READ usage (pair with COPY_DST and copy
  // into this buffer from your storage buffer). The wait uses
  // Instance::WaitAny, which this library's instance enables via the
  // TimedWaitAny feature at creation; external (Skia-provided) instances
  // without it fail the wait and throw rather than hang.
  size_t offset = o.value_or(0);
  size_t size = sizeIn.has_value()
                    ? sizeIn.value()
                    : static_cast<size_t>(_instance.GetSize() - offset);
  constexpr size_t kMaxReadSyncBytes = 1 << 20;
  if (size > kMaxReadSyncBytes) {
    throw std::runtime_error(
        "readSync is intended for small readbacks (<= 1 MiB); use mapAsync "
        "for large buffers");
  }
  wgpu::MapAsyncStatus mapStatus = wgpu::MapAsyncStatus::Error;
  std::string mapMessage = "callback never ran";
  auto future = _instance.MapAsync(
      wgpu::MapMode::Read, offset, size, wgpu::CallbackMode::WaitAnyOnly,
      [&mapStatus, &mapMessage](wgpu::MapAsyncStatus status,
                                wgpu::StringView message) {
        mapStatus = status;
        mapMessage = std::string(message);
      });
  constexpr uint64_t kTimeoutNs = 2'000'000'000; // 2s: a hung GPU, not a wait
  auto waitStatus = _async->instance().WaitAny(future, kTimeoutNs);
  if (waitStatus != wgpu::WaitStatus::Success) {
    throw std::runtime_error(
        "readSync: WaitAny did not complete (timeout, or the instance lacks "
        "the TimedWaitAny feature)");
  }
  if (mapStatus != wgpu::MapAsyncStatus::Success) {
    throw std::runtime_error("readSync: mapping failed: " + mapMessage);
  }
  const void *ptr = _instance.GetConstMappedRange(offset, size);
  if (ptr == nullptr) {
    _instance.Unmap();
    throw std::runtime_error("readSync: GetConstMappedRange failed");
  }
  auto result = std::make_shared<OwnedArrayBuffer>(size);
  memcpy(result->data(), ptr, size);
  _instance.Unmap();
  return result;
}

void GPUBuffer::destroy() { _instance.Destroy(); }

async::AsyncTaskHandle GPUBuffer::mapAsync(jsi::Runtime &runtime,
                                           uint64_t modeIn,
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
  auto bufferHandle = _instance;
  uint64_t resolvedOffset = offset.value_or(0);

  // Post to the CALLING runtime's context, not the one captured at buffer
  // creation (_async): the buffer may have been created on another runtime and
  // boxed across (e.g. device created on the main JS runtime, mapAsync called
  // from a worklet). The returned Promise lives on the calling runtime, so it
  // must be settled from that runtime's own thread — and postTask itself
  // schedules the pump through its context's runtime (setTimeout), which is
  // only safe for the runtime we are currently executing on.
  auto context =
      async::RuntimeContext::getOrCreate(runtime, _async->instance());
  return context->postTask(
      [bufferHandle, mode, resolvedOffset,
       rangeSize](const async::AsyncTaskHandle::ResolveFunction &resolve,
                  const async::AsyncTaskHandle::RejectFunction &reject) {
        bufferHandle.MapAsync(
            mode, resolvedOffset, rangeSize, wgpu::CallbackMode::AllowProcessEvents,
            [resolve, reject](wgpu::MapAsyncStatus status,
                              wgpu::StringView message) {
              switch (status) {
              case wgpu::MapAsyncStatus::Success:
                resolve(nullptr);
                break;
              case wgpu::MapAsyncStatus::CallbackCancelled:
                reject("MapAsyncStatus::CallbackCancelled");
                break;
              case wgpu::MapAsyncStatus::Error:
                reject("MapAsyncStatus::Error");
                break;
              case wgpu::MapAsyncStatus::Aborted:
                reject("MapAsyncStatus::Aborted");
                break;
              default:
                reject("MapAsyncStatus: " +
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
