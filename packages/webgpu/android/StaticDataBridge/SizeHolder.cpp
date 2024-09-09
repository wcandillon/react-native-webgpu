#include "SizeHolder.h"

namespace rnwgpu {

std::map<int, Size> SizeHolder::sizeHolder;

Size SizeHolder::getSize(const int contextId) {
  return sizeHolder[contextId];
}

void SizeHolder::setSize(const int contextId, const Size& size) {
  sizeHolder[contextId] = size;
}

void SizeHolder::eraseSize(const int contextId) {
  sizeHolder.erase(contextId);
}

}
