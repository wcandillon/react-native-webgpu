import _ from "lodash";
import { Node, type VariableDeclaration } from "ts-morph";

export const getEnum = (decl: VariableDeclaration) => {
  const name = decl.getName();
  const wname = name.substring(3);
  const properties = decl.getDescendants().filter(Node.isPropertySignature);
  return `#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class ${name} : public m::HybridObject {
public:
  ${name}() : HybridObject("${name}") {}

public:
  ${properties.map((property) => {
    return `wgpu::${wname} ${_.camelCase(property.getName())}() {
      return wgpu::${wname}::${property.getName()};
    }`;
  })}

  void loadHybridMethods() override {
    ${properties.map((property) => {
      return `registerHybridGetter("${property.getName()}", &${name}::${_.camelCase(property.getName())}, this)`;
    })}
  }
};
} // namespace rnwgpu`;
};
