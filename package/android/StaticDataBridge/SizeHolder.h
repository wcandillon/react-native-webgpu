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
  static Size getSize(int contextId);
  static void setSize(int contextId, const Size& size);

};

}
