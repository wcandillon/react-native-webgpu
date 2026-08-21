#include "GPUBuffer.h"

#include <cmath>
#include <cstring>
#include <limits>
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

std::shared_ptr<ArrayBuffer>
GPUBuffer::readbackSync(jsi::Runtime &runtime, std::optional<double> offsetIn,
                        std::optional<double> sizeIn,
                        std::optional<double> timeoutMsIn) {
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
  auto toByteSize = [&runtime](const char *name, double value) -> size_t {
    constexpr double kMaxSafeInteger = 9'007'199'254'740'991.0;
    if (!std::isfinite(value) || value < 0 || std::floor(value) != value ||
        value > kMaxSafeInteger ||
        value > static_cast<double>(std::numeric_limits<size_t>::max())) {
      throw jsi::JSError(runtime, std::string("GPUBuffer.readbackSync ") +
                                      name +
                                      " must be a non-negative safe integer");
    }
    return static_cast<size_t>(value);
  };

  const size_t offset =
      offsetIn.has_value() ? toByteSize("offset", *offsetIn) : 0;
  const uint64_t bufferSize = _instance.GetSize();
  if (offset > bufferSize) {
    throw jsi::JSError(runtime,
                       "GPUBuffer.readbackSync offset exceeds the buffer size");
  }
  const size_t size = sizeIn.has_value()
                          ? toByteSize("size", *sizeIn)
                          : static_cast<size_t>(bufferSize - offset);
  if (size > bufferSize - offset) {
    throw jsi::JSError(runtime,
                       "GPUBuffer.readbackSync range exceeds the buffer size");
  }

  constexpr size_t kMaxReadbackSyncBytes = 1 << 20;
  if (size > kMaxReadbackSyncBytes) {
    throw jsi::JSError(
        runtime,
        "GPUBuffer.readbackSync is limited to 1 MiB; use mapAsync for larger "
        "readbacks");
  }

  constexpr double kDefaultTimeoutMs = 2'000.0;
  constexpr double kNanosecondsPerMillisecond = 1'000'000.0;
  const double timeoutMs = timeoutMsIn.value_or(kDefaultTimeoutMs);
  const double maxTimeoutMs =
      static_cast<double>(std::numeric_limits<uint64_t>::max()) /
      kNanosecondsPerMillisecond;
  if (!std::isfinite(timeoutMs) || timeoutMs < 0 || timeoutMs > maxTimeoutMs) {
    throw jsi::JSError(
        runtime,
        "GPUBuffer.readbackSync timeoutMs must be a finite, non-negative "
        "number");
  }
  const uint64_t timeoutNs =
      static_cast<uint64_t>(timeoutMs * kNanosecondsPerMillisecond);

  struct MapResult {
    wgpu::MapAsyncStatus status = wgpu::MapAsyncStatus::Error;
    std::string message = "callback never ran";
  };
  auto mapResult = std::make_shared<MapResult>();
  auto future = _instance.MapAsync(
      wgpu::MapMode::Read, offset, size, wgpu::CallbackMode::WaitAnyOnly,
      [mapResult](wgpu::MapAsyncStatus status, wgpu::StringView message) {
        mapResult->status = status;
        mapResult->message = std::string(message);
      });
  auto waitStatus = _async->instance().WaitAny(future, timeoutNs);
  if (waitStatus != wgpu::WaitStatus::Success) {
    // Cancels the pending map request. The callback owns MapResult so a late
    // completion cannot access stack memory after this method returns.
    _instance.Unmap();
    throw jsi::JSError(
        runtime,
        "GPUBuffer.readbackSync did not complete before timeoutMs, or the Dawn "
        "instance does not support timed waits");
  }
  if (mapResult->status != wgpu::MapAsyncStatus::Success) {
    throw jsi::JSError(runtime, "GPUBuffer.readbackSync mapping failed: " +
                                    mapResult->message);
  }
  const void *ptr = _instance.GetConstMappedRange(offset, size);
  if (ptr == nullptr) {
    _instance.Unmap();
    throw jsi::JSError(runtime,
                       "GPUBuffer.readbackSync could not access the mapped "
                       "range");
  }
  // Allocate owned native storage. The JSI converter wraps this memory without
  // copying it again and reports its external memory pressure to the runtime.
  auto result = std::make_shared<ArrayBuffer>(size, 1);
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
