#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

static bool conv(const char *out, const std::string &in) {
  out = in.c_str();
  return true;
}

template <typename T, typename U>
static typename std::enable_if<
    std::is_arithmetic<T>::value && std::is_arithmetic<U>::value, bool>::type
conv(T &out, const U in) {
  out = static_cast<T>(in);
  return true;
}

template <typename EnumT>
static typename std::enable_if<std::is_enum<EnumT>::value, bool>::type
conv(EnumT &out, const double in) {
    out = static_cast<EnumT>(in);
    return true;
}

template <typename OuterT, typename InnerT>
static bool conv(OuterT &out, const std::optional<InnerT> &in) {
  if (in.has_value()) {
    return conv<OuterT, InnerT>(out, in.value());
  }
  return true;
}

template <typename OuterT, typename InnerT>
static bool conv(OuterT *out, std::size_t &size,
                 const std::vector<InnerT> &in) {
  size = in.size();
  std::vector<OuterT> outVector;
  for (std::size_t i = 0; i < size; i++) {
    if (!conv<OuterT, InnerT>(out[i], in[i])) {
      return false;
    }
  }
  out = outVector.data();
  return true;
}

template <typename T, typename = void> struct has_get : std::false_type {};

template <typename T>
struct has_get<T, std::void_t<decltype(std::declval<T>().get())>>
    : std::true_type {};

template <typename OuterT, typename InnerT>
static auto conv(OuterT &out, const std::shared_ptr<InnerT> &in)
    -> std::enable_if_t<has_get<InnerT>::value, bool> {
  return conv<OuterT, decltype(in->get())>(out, in->get());
}

} // namespace rnwgpu
