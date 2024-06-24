import type { InterfaceDeclaration } from "ts-morph";

const instanceAliases: Record<string, string> = {
  GPU: "Instance",
};
export const getHybridObject = (decl: InterfaceDeclaration) => {
  const name = decl.getName();
  const instanceName = `wgpu::${instanceAliases[name] || name.substring(3)}`;
  return `#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class ${name} : public m::HybridObject {
public:
  ${name}(std::shared_ptr<${instanceName}> instance) : HybridObject("${name}"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &${name}::getBrand, this);
  }

private:
  std::shared_ptr<${instanceName}> _instance;
};
} // namespace rnwgpu`;
};
