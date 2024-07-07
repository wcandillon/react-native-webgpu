import type { InterfaceDeclaration, PropertySignature } from "ts-morph";

import type { Union } from "./Unions";

const getBoolean = (name: string) => {
  return `if (${name}.isBool()) {
    result->_instance.${name} = ${name}.getBool();
}`;
};

const getNumber = (name: string) => {
  return `if (${name}.isNumber()) {
    result->_instance.${name} = ${name}.getNumber();
}`;
};

const getString = (name: string) => {
  return `if (${name}.isString()) {
    auto str = value.asString(runtime).utf8(runtime);
    result->_instance.${name} = str.c_str();
}`;
};

const propFromJSI = (
  className: string,
  prop: PropertySignature,
  _unions: Union[],
) => {
  const name = prop.getName();
  const isOptional = prop
    .getType()
    .getUnionTypes()
    .some((t) => t.isUndefined());
  return `if (value.hasProperty(runtime, "${name}")) {
  auto ${name} = value.getProperty(runtime, "${name}");
  ${prop.getType().isBoolean() ? getBoolean(name) : ""}
  ${prop.getType().isNumber() ? getNumber(name) : ""}
  ${prop.getType().isString() ? getString(name) : ""}
  ${
    !isOptional
      ? `if (${name}.isUndefined()) {
    throw std::runtime_error("Property ${className}::${name} is required");  
  }`
      : ""
  }
} ${
    !isOptional
      ? `else {
  throw std::runtime_error("Property ${className}::${name} is not defined");
}`
      : ""
  }`;
};

export const getDescriptor = (decl: InterfaceDeclaration, unions: Union[]) => {
  const name = decl.getName();
  const wgpuName = `wgpu::${name.substring(3)}`;
  return `#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

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

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::${name}>> {
  static std::shared_ptr<rnwgpu::${name}>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::${name}>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      ${decl
        .getProperties()
        .map((prop) => {
          return propFromJSI(name, prop, unions);
        })
        .join("\n")}
    }
    // else if () {
    // throw std::runtime_error("Expected an object for ${name}");
    //}
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
