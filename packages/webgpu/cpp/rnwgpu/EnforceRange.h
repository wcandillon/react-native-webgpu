//
//  EnforceRange.h
//  Pods
//
//  Created by Ritesh Shukla on 05/10/26.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace rnwgpu {

template <typename T> T enforceRange(double value, const char *name) {
  static_assert(std::is_integral_v<T>);
  constexpr double maxSafeInteger = 9007199254740991.0;
  constexpr double lower = std::max(
      static_cast<double>(std::numeric_limits<T>::min()), -maxSafeInteger);
  constexpr double upper = std::min(
      static_cast<double>(std::numeric_limits<T>::max()), maxSafeInteger);
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) + " must be a finite number");
  }
  double integer = std::trunc(value);
  if (integer < lower || integer > upper) {
    throw std::out_of_range(std::string(name) + " is out of range");
  }
  return static_cast<T>(integer);
}

} // namespace rnwgpu
