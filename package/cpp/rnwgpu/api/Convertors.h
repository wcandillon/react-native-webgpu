#pragma once

#include <vector>

namespace rnwgpu {

template <typename I> auto conv(const std::vector<std::shared_ptr<I>> &input) {
  std::vector<decltype(std::declval<I>().get())> result;
  result.reserve(input.size());
  for (const auto &ptr : input) {
    result.push_back(ptr->get());
  }
  return result;
}

} // namespace rnwgpu