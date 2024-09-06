#pragma once

#include <map>

namespace rnwgpu {

extern int a;

struct Size {
  float width;
  float height;
};

class SizeHolder {

  static std::map<int, Size> sizeHolder;
  static int tmp;

public:
  static Size getSize(int contextId);
  static void setSize(int contextId, Size size);

};

}
