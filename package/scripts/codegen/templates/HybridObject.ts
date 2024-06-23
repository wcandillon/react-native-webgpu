import type { InterfaceDeclaration } from "ts-morph";

export const getHybridObject = (decl: InterfaceDeclaration) => {
  const name = decl.getName();
  return `#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class ${name} : public m::HybridObject {
public:
  ${name}() : HybridObject("${name}") {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPU::getBrand, this);
  }
};
} // namespace rnwgpu`;
};
