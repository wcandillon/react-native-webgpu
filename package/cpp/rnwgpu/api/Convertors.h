#pragma once

#include <memory>
#include <vector>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

//template <typename I> auto conv(const std::vector<std::shared_ptr<I>> &input) {
//  std::vector<decltype(std::declval<I>().get())> result;
//  result.reserve(input.size());
//  for (const auto &ptr : input) {
//    result.push_back(ptr->get());
//  }
//  return result;
//}

bool conv(bool &out, const bool &in) {
    out = in;
    return true;
}

bool conv(uint32_t &out, const double &in) {
    out = static_cast<uint32_t>(in);
    return true;
}

template <typename InnerT, typename OuterT> bool conv(InnerT &out, const std::optional<OuterT> &in) {
    if (in.has_value()) {
      return conv(out, in.value());
    }
    return true;
}

bool conv(wgpu::RequestAdapterOptions& out, const GPURequestAdapterOptions& in) {
  return conv(out.powerPreference, in.powerPreference) && conv(out.forceFallbackAdapter, in.forceFallbackAdapter);
}

} // namespace rnwgpu
