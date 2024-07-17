#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {


bool conv(const char *&out, const std::string &in);

bool conv(double &out, double in);

template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value, bool>::type
conv(T &out, double in);

template <typename T>
typename std::enable_if<std::is_assignable<T &, std::nullptr_t>::value,
                        bool>::type
conv(T &out, std::nullptr_t in);

template <typename InnerT, typename OuterT>
bool conv(InnerT *&out, const std::vector<OuterT> &in);

template <typename InnerT, typename OuterT>
bool conv(InnerT &out, const std::optional<OuterT> &in);

template <typename T, typename = void> struct has_get : std::false_type {};

template <typename T>
struct has_get<T, std::void_t<decltype(std::declval<T>().get())>>
    : std::true_type {};

template <typename InnerT, typename OuterT>
bool conv(InnerT &out, const std::shared_ptr<OuterT> &in);

template <typename InnerT, typename... OuterTs>
bool conv(InnerT &out, const std::variant<OuterTs...> &in);

template <typename T> bool conv(T &in, T &out);

template <typename OutT>
typename std::enable_if<std::is_enum<OutT>::value, bool>::type
conv(OutT &out, const double &in);

} // namespace rnwgpu
