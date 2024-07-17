#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

bool conv(bool &out, const bool &in) {
  out = in;
  return true;
}

bool conv(const char *&out, const std::string &in) {
  out = in.c_str();
  return true;
}

bool conv(uint64_t &out, const double &in) {
  out = static_cast<uint64_t>(in);
  return true;
}

bool conv(uint32_t &out, const double &in) {
  out = static_cast<uint32_t>(in);
  return true;
}

template <typename InnerT, typename OuterT>
bool conv(InnerT *&out, const std::vector<OuterT> &in) {
  std::vector<InnerT> result;
  result.reserve(in.size());
  for (const auto &item : in) {
    InnerT converted;
    if (!conv(converted, item)) {
      return false;
    }
    result.push_back(converted);
  }
  out = result.data();
  return true;
}

template <typename InnerT, typename OuterT>
bool conv(InnerT &out, const std::optional<OuterT> &in) {
  if (in.has_value()) {
    return conv(out, in.value());
  }
  return true;
}

} // namespace rnwgpu
