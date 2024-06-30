import { InterfaceDeclaration, PropertySignature } from "ts-morph";

const propFromJSI = (className: string, prop: PropertySignature) => {
  const name = prop.getName();
  const isOptional = prop.getType().getText().includes("undefined");
  return `if (value.hasProperty(runtime, "${name}")) {
  auto ${name} = value.getProperty(runtime, "${name}");
  if (${name}.isNumber()) {
    result->_instance.${name} = ${name}.getNumber();
  }
  ${!isOptional ? `else if (${name}.isNull() || ${name}.isUndefined()) {
    throw std::runtime_error("Property ${className}::${name} is required");  
  }` : ""}
}`;
};

export const getDescriptor = (decl: InterfaceDeclaration) => {
  const name = decl.getName();
  const wgpuName = `wgpu::${name.substring(3)}`;
  return `#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class ${name} {
  public:
    ${wgpuName}* getInstance() {
      return &_instance;
    }

  private:
    ${wgpuName} _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::${name}>> {
  static std::shared_ptr<rnwgpu::${name}>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::${name}>();
    ${decl.getProperties().map((prop) => {
      console.log(prop.getName());
      return propFromJSI(name, prop);
    }).join("\n")}
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::${name}> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
`;
};
