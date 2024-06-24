import _ from "lodash";
import type { PropertySignature, VariableDeclaration } from "ts-morph";
import { Node } from "ts-morph";

const enumAliases: Record<string, string> = {
  ColorWrite: "ColorWriteMask",
};

const getPropName = (prop: PropertySignature) => {
  const name = _.upperFirst(_.camelCase(prop.getName()));
  return name;
};

export const getEnum = (decl: VariableDeclaration) => {
  const name = decl.getName();
  const wname = enumAliases[name.substring(3)] || name.substring(3);
  const properties = decl.getDescendants().filter(Node.isPropertySignature);
  return `#pragma once
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class ${name} : public m::HybridObject {
public:
  ${name}() : HybridObject("${name}") {}

public:
  ${properties
    .map((property) => {
      const prop = getPropName(property);
      return `double ${prop}() {
      return static_cast<double>(wgpu::${wname}::${prop});
    }`;
    })
    .join("\n    ")}

  void loadHybridMethods() override {
    ${properties
      .map((property) => {
        const prop = getPropName(property);
        return `registerHybridGetter("${property.getName()}", &${name}::${prop}, this);`;
      })
      .join("\n    ")}
  }
};
} // namespace rnwgpu`;
};
