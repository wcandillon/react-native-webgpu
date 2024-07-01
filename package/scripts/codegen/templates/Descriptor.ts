import { InterfaceDeclaration, PropertySignature } from "ts-morph";

const getBoolean = (name: string) => {
  return `if (value.hasProperty(runtime, "${name}")) {
    result->_instance.${name} = ${name}.getBool();
}`;
};

const getNumber = (name: string) => {
  return `if (value.hasProperty(runtime, "${name}")) {
    result->_instance.${name} = ${name}.getNumber();
}`;
};

const getString = (name: string) => {
  return `if (value.hasProperty(runtime, "${name}")) {
    auto str = value.asString(runtime).utf8(runtime);
    result->_instance.${name} = str.c_str();
}`;
};

const getEnum = (values: string[]) => {
  console.log(values);
  return ``;
};

const propFromJSI = (className: string, prop: PropertySignature) => {
  const name = prop.getName();
  const isOptional = prop.getType().getText().includes("undefined");
  const possibleTypes = prop.getType().getUnionTypes().map(t => t.getText()).filter((t) => t != "undefined");
  const enumValues = possibleTypes.filter((t) => t.startsWith('"'));
  //console.log(possibleTypes);
  return `if (value.hasProperty(runtime, "${name}")) {
  auto ${name} = value.getProperty(runtime, "${name}");
  ${possibleTypes.includes("true") || possibleTypes.includes("false") ? getBoolean(name) : ""}
  ${possibleTypes.includes("number") ? getNumber(name) : ""}
  ${possibleTypes.includes("string") ? getString(name) : ""}
  ${enumValues.length > 0 ? getEnum(enumValues.map(v => v.substring(1, v.length-1))) : ""}
  ${!isOptional ? `else if (${name}.isUndefined()) {
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

    ${wgpuName} _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::${name}>> {
  static std::shared_ptr<rnwgpu::${name}>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto value = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::${name}>();
    ${decl.getProperties().map((prop) => {
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
