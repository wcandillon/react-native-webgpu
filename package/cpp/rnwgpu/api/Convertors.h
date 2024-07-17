#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

class GPUBindGroupEntry;

bool conv(const char *&out, const std::string &in) {
  out = in.c_str();
  return true;
}

template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value, bool>::type
conv(T &out, double in) {
  out = static_cast<T>(in);
  return true;
}

template <typename T>
typename std::enable_if<std::is_assignable<T &, std::nullptr_t>::value,
                        bool>::type
conv(T &out, std::nullptr_t in) {
  out = nullptr;
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
    return conv<InnerT, OuterT>(out, in.value());
  }
  return true;
}

template <typename InnerT, typename OuterT>
bool conv(InnerT &out, const std::shared_ptr<OuterT> &in) {
  if constexpr (std::is_member_function_pointer_v<decltype(&OuterT::get)>) {
    return conv(out, in->get());
  } else {
    return conv(out, in.get());
  }
}

template <typename InnerT, typename... OuterTs>
bool conv(InnerT &out, const std::variant<OuterTs...> &in) {
  return std::visit([&out](const auto &value) { return conv(out, value); }, in);
}

template <typename T>
bool conv(T &in, T &out) {
    in = out;
    return true;
}

template <typename OutT>
typename std::enable_if<std::is_enum<OutT>::value, bool>::type
conv(OutT &out, const double &in) {
  out = static_cast<OutT>(in);
  return true;
}

} // namespace rnwgpu
