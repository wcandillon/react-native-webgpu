#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

class GPUBindGroupEntry;

bool conv(bool &out, const bool &in) {
  out = in;
  return true;
}

bool conv(const char *&out, const std::string &in) {
  out = in.c_str();
  return true;
}

template <typename T> bool conv(T &out, double in) {
  *out = static_cast<T>(in);
  return true;
}

template <typename T> bool conv(T &out, std::nullptr_t in) {
  *out = nullptr;
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

template <typename InnerT, typename... OuterTs>
bool conv(InnerT *&out, const std::variant<OuterTs...> &in) {
  return std::visit(
      [&out](const auto &value) {
        InnerT converted;
        if (!conv(converted, value)) {
          return false;
        }
        out = new InnerT(std::move(converted));
        return true;
      },
      in);
}

template <typename InnerT, typename OuterT>
bool conv(InnerT &out, const std::optional<OuterT> &in) {
  if (in.has_value()) {
    return conv(out, in.value());
  }
  return true;
}

template <typename InnerT, typename OuterT>
bool conv(InnerT &out, const std::shared_ptr<OuterT> &in) {
  out = in->get();
  return true;
}

} // namespace rnwgpu
