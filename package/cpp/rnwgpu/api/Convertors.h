#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPURequestAdapterOptions.h"
#include "GPUBindGroupDescriptor.h"

namespace rnwgpu {

class Convertor {
public:
  ~Convertor() {
    for (auto &free : free_) {
      free();
    }
  }

  template <typename OUT, typename IN>
  [[nodiscard]] inline bool operator()(OUT &&out, IN &&in) {
    return Convert(std::forward<OUT>(out), std::forward<IN>(in));
  }

  template <typename T> [[nodiscard]] bool Convert(T &out, const T &in) {
    out = in;
    return true;
  }

  template <typename OUT, typename IN>
  [[nodiscard]] bool Convert(OUT &out, const std::optional<IN> &in) {
    if (in.has_value()) {
      return Convert(out, in.value());
    }
    return true;
  }

  [[nodiscard]] bool Convert(wgpu::RequestAdapterOptions &out,
                             const GPURequestAdapterOptions &in) {
    return Convert(out.powerPreference, in.powerPreference) &&
           Convert(out.forceFallbackAdapter, in.forceFallbackAdapter);
  }

private:
  template <typename T> T *Allocate(size_t n = 1) {
    auto *ptr = new T[n]{};
    free_.emplace_back([ptr] { delete[] ptr; });
    return ptr;
  }

  std::vector<std::function<void()>> free_;
};

} // namespace rnwgpu
