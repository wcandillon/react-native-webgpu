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

#include <NativeObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class ${name} : public NativeObject<${name}> {
public:
  static constexpr const char *CLASS_NAME = "${name}";

  ${name}() : NativeObject(CLASS_NAME) {}

public:
  ${properties
    .map((property) => {
      const prop = getPropName(property);
      return `double ${prop}() {
      return static_cast<double>(wgpu::${wname}::${prop});
    }`;
    })
    .join("\n    ")}

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    ${properties
      .map((property) => {
        const prop = getPropName(property);
        return `installGetter(runtime, prototype, "${property.getName()}", &${name}::${prop});`;
      })
      .join("\n    ")}
  }
};
} // namespace rnwgpu`;
};
