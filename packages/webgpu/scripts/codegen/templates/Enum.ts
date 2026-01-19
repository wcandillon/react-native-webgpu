import _ from "lodash";
import type { InterfaceDeclaration, PropertySignature } from "ts-morph";

const enumAliases: Record<string, string> = {
  ColorWrite: "ColorWriteMask",
};

const getPropName = (prop: PropertySignature) => {
  const name = _.upperFirst(_.camelCase(prop.getName()));
  return name;
};

export const getEnum = (decl: InterfaceDeclaration) => {
  const name = decl.getName();
  const wname = enumAliases[name.substring(3)] || name.substring(3);
  const properties = decl.getProperties();
  return `#pragma once

#include <jsi/jsi.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class ${name} {
public:
  static jsi::Object create(jsi::Runtime &runtime) {
    jsi::Object obj(runtime);
    ${properties
      .map((property) => {
        const prop = getPropName(property);
        return `obj.setProperty(runtime, "${property.getName()}",
                    static_cast<double>(wgpu::${wname}::${prop}));`;
      })
      .join("\n    ")}
    return obj;
  }
};

} // namespace rnwgpu
`;
};
