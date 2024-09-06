#include "SizeHolder.h"

namespace rnwgpu {

std::map<int, Size> SizeHolder::sizeHolder;
int SizeHolder::tmp = 7;

Size SizeHolder::getSize(int contextId) {
  return sizeHolder[contextId];
}

void SizeHolder::setSize(int contextId, Size size) {
  sizeHolder[contextId] = size;
}

}
