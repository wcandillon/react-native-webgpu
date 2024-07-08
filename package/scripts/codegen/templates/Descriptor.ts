/* eslint-disable max-len */
import type { InterfaceDeclaration, PropertySignature } from "ts-morph";

import type { Union } from "./Unions";

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
    auto str = value.asString(runtime).utf8(runtime);
    result->_instance.${name} = str.c_str();
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
  const enumLabel = prop.getTypeNode()?.getText();
  const isEnum =
    !!enumLabel?.startsWith("GPU") && !enumsToSkip.includes(enumLabel);
  return `if (value.hasProperty(runtime, "${name}")) {
  auto ${name} = value.getProperty(runtime, "${name}");
  ${prop.getType().isBoolean() ? getBoolean(name) : ""}
  ${prop.getType().isNumber() ? getNumber(name, isEnum ? prop.getTypeNode()?.getText() : undefined) : ""}
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

const mergeParentInterfaces = (interfaceDecl: InterfaceDeclaration) => {
  const parentInterfaces = interfaceDecl.getBaseDeclarations();

  for (const parentInterface of parentInterfaces) {
    const parentDeclaration = parentInterface
      .getType()
      .getSymbol()
      ?.getDeclarations()[0];

    if (parentDeclaration) {
      const parentInterfaceDecl = parentDeclaration as InterfaceDeclaration;

      // Recursively merge parent interfaces
      const mergedParentInterface = mergeParentInterfaces(parentInterfaceDecl);

      // Merge properties from parent to child
      for (const prop of mergedParentInterface.getProperties()) {
        if (!interfaceDecl.getProperty(prop.getName())) {
          interfaceDecl.addProperty(prop.getStructure());
        }
      }

      // Merge methods from parent to child
      for (const method of mergedParentInterface.getMethods()) {
        if (!interfaceDecl.getMethod(method.getName())) {
          interfaceDecl.addMethod(method.getStructure());
        }
      }
    }
  }

  return interfaceDecl;
};

export const getDescriptor = (decl: InterfaceDeclaration, unions: Union[]) => {
  mergeParentInterfaces(decl);
  const name = decl.getName();
  const wgpuName = `wgpu::${name.substring(3)}`;

  //decl.getType(
  return `#pragma once

#include <memory>

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
