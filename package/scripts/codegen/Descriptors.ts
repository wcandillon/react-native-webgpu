import type { InterfaceDeclaration, Type, PropertySignature } from "ts-morph";
import { SyntaxKind } from "ts-morph";

import { debugType, mergeParentInterfaces } from "./templates/common";

const layout = {
  type: "std::variant<std::null_ptr, std::shared_ptr<GPUPipelineLayout>>",
  dependencies: ["GPUPipelineLayout"],
};

const colorFormats = {
  type: "std::vector<std::variant<wgpu::TextureFormat, std::nullptr_t>>",
  dependencies: ["vector"],
};

const resolved: Record<
  string,
  Record<string, { type: string; dependencies: string[] }>
> = {
  GPUComputePipelineDescriptor: {
    layout,
  },
  GPURenderPipelineDescriptor: {
    layout,
  },
  GPUShaderModuleCompilationHint: {
    layout,
  },
  GPUDeviceDescriptor: {
    defaultQueue: {
      type: "std::shared_ptr<GPUQueueDescriptor>",
      dependencies: ["GPUQueueDescriptor"],
    },
  },
  GPURenderPassLayout: {
    colorFormats,
  },
  GPURenderBundleEncoderDescriptor: {
    colorFormats,
  },
};

interface ResolveTypeState {
  dependencies: Set<string>;
  prop: PropertySignature;
}

const resolveType = (type: Type, state: ResolveTypeState): string => {
  const { dependencies, prop } = state;
  const propName = prop.getName();
  const className =
    prop.getFirstAncestorByKind(SyntaxKind.InterfaceDeclaration)?.getName() ??
    "";
  const symbol = type.getSymbol();
  if (resolved[className] && resolved[className][propName]) {
    resolved[className][propName].dependencies.forEach((d) =>
      dependencies.add(d),
    );
    return resolved[className][propName].type;
  } else if (type.isString()) {
    dependencies.add("string");
    return "std::string";
  } else if (type.isBoolean() || type.isBooleanLiteral()) {
    return "bool";
  } else if (type.isNumber()) {
    return "double";
  } else if (type.isArray()) {
    dependencies.add("vector");
    return `std::vector<${resolveType(type.getArrayElementType()!, state)}>`;
  } else if (type.isUnion()) {
    const unionTypes = type.getUnionTypes().filter((t) => !t.isUndefined());
    if (unionTypes.length === 1) {
      return resolveType(unionTypes[0], state);
    } else if (unionTypes.every((t) => t.isStringLiteral())) {
      let name = type.getAliasSymbol()?.getName();
      if (!name) {
        name = prop.getTypeNode()?.getText();
        if (!name) {
          console.log(name);
          console.log(debugType(type));
          throw new Error(
            `${className}.${propName} not handled with string literal union`,
          );
        }
      }
      return `wgpu::${name.substring(3)}`;
    } else {
      const unionNames = Array.from(
        new Set(unionTypes.map((t) => resolveType(t, state))),
      );
      if (unionNames.length === 1) {
        return unionNames[0];
      } else {
        dependencies.add("variant");
        return `std::variant<${unionNames.join(", ")}>`;
      }
    }
  } else if (type.isNull()) {
    return "std::nullptr_t";
  } else if (type.isInterface()) {
    const name = type.getSymbol()?.getName() ?? "";
    if (name === "GPUObjectDescriptorBase") {
      throw new Error(
        `${className}.${propName} not handled with GPUObjectDescriptorBase`,
      );
    }
    dependencies.add(name);
    return `std::shared_ptr<${name}>`;
  } else if (type?.getText().startsWith("Record<")) {
    const args = type
      .getAliasTypeArguments()
      .map((arg) => resolveType(arg, state));
    dependencies.add("map");
    return `std::map<${args[0]}, ${args[1]}>`;
  } else if (symbol && symbol.getName() === "Iterable") {
    const args = type.getTypeArguments().map((arg) => resolveType(arg, state));
    dependencies.add("vector");
    return `std::vector<${args.length === 1 ? args[0] : `std::variant<${args.join(", ")}>`}>`;
  }
  //return "unknown";
  console.log(JSON.stringify(debugType(type), null, 2));
  throw new Error(`Unhandled ${className}::${propName}`);
};

interface Prop {
  name: string;
  type: string;
  debug: string;
}

const jsiProp = ({ name, type }: Prop) => {
  return `if (value.hasProperty(runtime, "${name}")) {
  auto prop = value.getProperty(runtime, "${name}");
  result->${name} = JSIConverter<${type}>::fromJSI(runtime, prop, false);
}`;
};

export const getDescriptor = (decl: InterfaceDeclaration) => {
  mergeParentInterfaces(decl);
  const name = decl.getName();
  const dependencies = new Set<string>();
  // we filter sourceMap?: any; for now
  const props = decl
    .getProperties()
    .filter((p) => !p.getType().isAny())
    .map((prop) => {
      const mandatoryType = resolveType(prop.getType(), { prop, dependencies });
      const type = prop.hasQuestionToken()
        ? `std::optional<${mandatoryType}>`
        : mandatoryType;
      if (prop.hasQuestionToken()) {
        dependencies.add("optional");
      }
      const debug = prop.getTypeNode()?.getText() ?? "";
      return {
        name: prop.getName(),
        type,
        debug,
      };
    });
  return `#pragma once

#include <memory>
${Array.from(dependencies)
  .filter((dep) => dep[0] === dep[0].toLowerCase())
  .map((dep) => `#include <${dep}>`)
  .join("\n")}

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include "RNFHybridObject.h"
#include "Convertors.h"
${Array.from(dependencies)
  .filter((dep) => dep[0] !== dep[0].toLowerCase())
  .map((dep) => `#include "${dep}.h"`)
  .join("\n")}

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct ${name} {
  ${props
    .map((p) => {
      const debug = p.debug.replace(/\s+/g, " ").trim();
      return `${p.type} ${p.name}; // ${debug}`;
    })
    .join("\n  ")}
};

${
  !customConv.includes(name)
    ? `
bool conv(wgpu::${name.substring(3)} &out,
          const ${name} &in) {
  ${props
    .filter((p) => p.type.startsWith("std::vector"))
    .map((p) => `out.${singular(p.name)}Count = in.${p.name}.size();`)
    .join("\n")}
  return ${props.map((p) => `conv(out.${p.name}, in.${p.name})`).join(" &&")};
}`
    : ""
}

} // namespace rnwgpu
 
namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::${name}>> {
  static std::shared_ptr<rnwgpu::${name}>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::${name}>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      ${props.map((prop) => jsiProp(prop)).join("\n")}
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::${name}> arg) {
    throw std::runtime_error("Invalid ${name}::toJSI()");
  }
};

} // namespace margelo`;
};

const singular = (word: string) => {
  return word === "entries" ? "entry" : word;
};

const customConv: string[] = []; //"GPUBindGroupEntry"
