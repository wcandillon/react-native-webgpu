/* eslint-disable max-len */
import type { InterfaceDeclaration, PropertySignature } from "ts-morph";

import type { Union } from "./Unions";
import { mergeParentInterfaces } from "./common";

const enumMap: Record<string, string> = {
  GPUBufferUsageFlags: "BufferUsage",
};

const enumMap2: Record<string, string> = {
  GPUStencilValue: "uint32_t",
  GPUDepthBias: "int32_t",
  GPUSampleMask: "uint32_t",
  GPUSize32: "uint32_t",
};

const getEnumName = (name: string) => {
  return enumMap2[name] ?? `wgpu::${enumMap[name] || name.substring(3)}`;
};

const getBoolean = (name: string) => {
  // TODO: unclippedDepth
  if (name === "unclippedDepth") {
    return "";
  }
  return `if (${name}.isBool()) {
    result->_instance.${name} = ${name}.getBool();
}`;
};

const getNumber = (name: string, enumName: string | undefined) => {
  return `if (${name}.isNumber()) {
    result->_instance.${name} = ${enumName ? `static_cast<${getEnumName(enumName)}>(${name}.getNumber())` : `${name}.getNumber()`};
}`;
};

const getUnion = (name: string, enumName: string) => {
  return `if (${name}.isString()) {
    auto str = ${name}.asString(runtime).utf8(runtime);
    ${getEnumName(enumName)} enumValue;
    m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
    result->_instance.${name} = enumValue;
}`;
};

const getString = (name: string, setOnInstance = true) => {
  return `if (${name}.isString()) {
    auto str = ${name}.asString(runtime).utf8(runtime);
    result->${name} = str;
    ${setOnInstance ? `result->_instance.${name} = result->${name}.c_str();` : ""}
}`;
};

const objectInstance: Record<string, true> = {
  GPUShaderModule: true,
};

const isDescriptorPtr: Record<string, true> = {
  GPUFragmentState: true,
  GPUDepthStencilState: true,
};

const getDescriptorObject = (
  name: string,
  alias: string,
  dependencies: string[],
) => {
  dependencies.push(alias);
  if (objectInstance[alias]) {
    return `if (${name}.isObject()) {
    result->_instance.${name} = value.asHostObject<rnwgpu::${alias}>(runtime)->get();
  }`;
  }
  return `if (${name}.isObject()) {
    auto val = m::JSIConverter<rnwgpu::${alias}>::fromJSI(runtime, ${name}, false);
    result->_instance.${name} =  val${isDescriptorPtr[alias] ? ".getInstance()" : "._instance"};
  }`;
};

const getAutoLayout = () => {
  return `if (layout.isString()) {
    auto str = layout.asString(runtime).utf8(runtime);
    if (str == "auto") {
      result->_instance.layout = nullptr;
    }
  }`;
};

const enumsToSkip = ["GPUSize64"];
const unionsToSkip = ["layout"];

// const logProp = (
//   className: string,
//   prop: PropertySignature,
//   _unions: Union[],
// ) => {
//   return `rnwgpu::Logger::logToConsole("${className}::${prop.getName()} = %f", result->_instance.${prop.getName()});`;
// };

/*
        if (layout.isString()) {
          auto str = layout.asString(runtime).utf8(runtime);
          if (str == "auto") {
            result->_instance.layout = nullptr;
          }
        }
          */

const propFromJSI = (
  className: string,
  prop: PropertySignature,
  _unions: Union[],
  dependencies: string[],
) => {
  const name = prop.getName();
  const types = prop.getType().isUnion()
    ? prop.getType().getUnionTypes()
    : [prop.getType()];
  const alias = prop.getTypeNode()?.getText();
  const isOptional =
    types.some((t) => t.isUndefined()) || prop.hasQuestionToken();
  const isBoolean = types.some(
    (t) => t.isBoolean() || t.getText() === "false" || t.getText() === "true",
  );
  const isNumber = types.some((t) => t.isNumber());
  const isString = types.some((t) => t.isString());
  const isUnion =
    alias &&
    types.some((t) => t.isStringLiteral()) &&
    !unionsToSkip.includes(prop.getName());
  const isAutoLayout =
    alias &&
    types.some((t) => t.isStringLiteral()) &&
    prop.getName() === "layout";
  const isEnum = !!alias?.startsWith("GPU") && !enumsToSkip.includes(alias);
  const labels = types.map((t) => t.getText());
  const isDescriptor =
    alias && alias.startsWith("GPU") && !isNumber && !isUnion;
  // const isDescriptor =
  //   !isNumber && !!prop.getType().getTypeNode()?.getName().startsWith("GPU");
  if (
    !isString &&
    !isBoolean &&
    !isNumber &&
    !isUnion &&
    !isAutoLayout &&
    !isDescriptor
  ) {
    const symbols = labels; //.filter((l) => !l.includes("import("));
    console.log("unhandled prop:");
    console.log({
      [prop.getName()]: symbols,
    });
  }
  return `if (value.hasProperty(runtime, "${name}")) {
  auto ${name} = value.getProperty(runtime, "${name}");
  ${isBoolean ? getBoolean(name) : ""}
  ${isNumber ? getNumber(name, isEnum ? alias : undefined) : ""}
  ${isUnion ? getUnion(name, alias) : ""}
  ${isString ? getString(name, name === "label") : ""}
  ${isAutoLayout ? getAutoLayout() : ""}
  ${isDescriptor ? getDescriptorObject(name, alias, dependencies) : ""}
  ${
    // !isBoolean && !isNumber && !isString && !isOptional
    //   ? (() => {
    //       console.log(prop.getText());
    //       return "";
    //     })()
    //   : ""
    ""
  }
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
  const dependencies: string[] = [];
  const name = decl.getName();
  const wgpuName = `wgpu::${name.substring(3)}`;
  const propsToHold = decl
    .getProperties()
    .filter((prop) => {
      return (
        prop
          .getType()
          .getUnionTypes()
          .some((t) => t.isString()) || prop.getType().isString()
      );
    })
    .map((prop) => {
      return `std::string ${prop.getName()};`;
    });
  const properties = decl.getProperties().map((prop) => {
    return propFromJSI(name, prop, unions, dependencies);
  });
  //decl.getType(
  return `#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>
#include "Logger.h"
#include "RNFJSIConverter.h"

${dependencies.map((d) => `#include "${d}.h"`).join("\n")}

namespace jsi = facebook::jsi;
namespace m = margelo;

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
      ${properties.join("\n")}
    }
    ${
      /* decl
      .getProperties()
      .map((prop) => {
        return logProp(name, prop, unions);
      })
      .join("\n") */ ""
    }
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
