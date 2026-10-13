#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace rnwgpu {

// Thrown when a JS number cannot be converted to a WebIDL [EnforceRange]
// integer. The host function wrappers in NativeObject rethrow it as a JS
// TypeError, which is what browsers throw for the same input.
class RangeTypeError : public std::invalid_argument {
public:
  using std::invalid_argument::invalid_argument;
};

namespace detail {

// The WebIDL name of the integer type, for the same message Chrome produces.
template <typename T> constexpr const char *webIDLIntegerName() {
  if constexpr (sizeof(T) == 1) {
    return std::is_signed_v<T> ? "byte" : "octet";
  } else if constexpr (sizeof(T) == 2) {
    return std::is_signed_v<T> ? "short" : "unsigned short";
  } else if constexpr (sizeof(T) == 4) {
    return std::is_signed_v<T> ? "long" : "unsigned long";
  } else {
    return std::is_signed_v<T> ? "long long" : "unsigned long long";
  }
}

} // namespace detail

// WebIDL [EnforceRange] conversion of a JS number to an integer
// (https://webidl.spec.whatwg.org/#abstract-opdef-converttoint): non-finite
// values and values outside the type's range are rejected, fractions are
// truncated toward zero. 64-bit types are limited to the integers a double
// holds exactly, [-(2^53 - 1), 2^53 - 1], as the spec requires.
//
// WebGPU marks nearly every integer it takes [EnforceRange] (GPUSize32,
// GPUIndex32, GPUSize64, GPUIntegerCoordinate, GPUSignedOffset32,
// GPUStencilValue, GPUSampleMask, GPUDepthBias). The flag constants
// (GPUBufferUsageFlags and friends) are plain `unsigned long` and are not
// converted here.
template <typename T> T enforceRange(double value) {
  static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
                "enforceRange converts to integer types only");
  constexpr double kMaxSafeInteger = 9007199254740991.0;
  constexpr double kLower = std::max(
      static_cast<double>(std::numeric_limits<T>::min()), -kMaxSafeInteger);
  constexpr double kUpper = std::min(
      static_cast<double>(std::numeric_limits<T>::max()), kMaxSafeInteger);
  if (!std::isfinite(value)) {
    throw RangeTypeError("The provided double value is non-finite.");
  }
  const double integer = std::trunc(value);
  if (integer < kLower || integer > kUpper) {
    throw RangeTypeError(std::string("Value is outside the '") +
                         detail::webIDLIntegerName<T>() + "' value range.");
  }
  return static_cast<T>(integer);
}

} // namespace rnwgpu
