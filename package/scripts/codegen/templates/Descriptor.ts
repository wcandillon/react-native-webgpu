/* eslint-disable max-len */
import type { InterfaceDeclaration, PropertySignature } from "ts-morph";

import type { Union } from "./Unions";
import { mergeParentInterfaces } from "./common";

const enumMap: Record<string, string> = {
  GPUBufferUsageFlags: "BufferUsage",
};

const getEnumName = (name: string) => {
  return `wgpu::${enumMap[name] || name.substring(3)}`;
};

const getBoolean = (name: string) => {
  return `if (${name}.isBool()) {
    result->_instance.${name} = ${name}.getBool();
}`;
};

const getNumber = (name: string, enumName: string | undefined) => {
  return `if (${name}.isNumber()) {
    result->_instance.${name} = ${enumName ? `static_cast<${getEnumName(enumName)}>(${name}.getNumber())` : `${name}.getNumber()`};
}`;
};

const getString = (name: string) => {
  return `if (${name}.isString()) {
    auto str = ${name}.asString(runtime).utf8(runtime);
    result->${name} = str;
    result->_instance.${name} = result->${name}.c_str();
}`;
};

const enumsToSkip = ["GPUSize64"];

const logProp = (
  className: string,
  prop: PropertySignature,
  _unions: Union[],
) => {
  return `rnwgpu::Logger::logToConsole("${className}::${prop.getName()} = %f", result->_instance.${prop.getName()});`;
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
  const isBoolean = prop
    .getType()
    .getUnionTypes()
    .some((t) => t.isBoolean());
  const isNumber = prop
    .getType()
    .getUnionTypes()
    .some((t) => t.isNumber());
  const isString = prop
    .getType()
    .getUnionTypes()
    .some((t) => t.isString());
  const enumLabel = prop.getTypeNode()?.getText();
  const isEnum =
    !!enumLabel?.startsWith("GPU") && !enumsToSkip.includes(enumLabel);
  return `if (value.hasProperty(runtime, "${name}")) {
  auto ${name} = value.getProperty(runtime, "${name}");
  ${isBoolean ? getBoolean(name) : ""}
  ${isNumber ? getNumber(name, isEnum ? prop.getTypeNode()?.getText() : undefined) : ""}
  ${isString ? getString(name) : ""}
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
  mergeParentInterfaces(decl);
  const name = decl.getName();
  const wgpuName = `wgpu::${name.substring(3)}`;
  const propsToHold = decl
    .getProperties()
    .filter((prop) => {
      return prop
        .getType()
        .getUnionTypes()
        .some((t) => t.isString());
    })
    .map((prop) => {
      return `std::string ${prop.getName()};`;
    });
  //decl.getType(
  return `#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>
#include "Logger.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

class ${name} {
  public:
    ${wgpuName}* getInstance() {
      return &_instance;
    }

    ${wgpuName} _instance;
  
    ${propsToHold.join("\n")}
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
    ${decl
      .getProperties()
      .map((prop) => {
        return logProp(name, prop, unions);
      })
      .join("\n")}
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
