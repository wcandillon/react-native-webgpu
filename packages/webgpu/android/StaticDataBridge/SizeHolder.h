#pragma once

#include <map>

namespace rnwgpu {

struct Size {
  float width;
  float height;
};

class SizeHolder {

  static std::map<int, Size> sizeHolder;

public:
  static Size getSize(const int contextId);
  static void setSize(const int contextId, const Size& size);
  static void eraseSize(const int contextId);

};

}
