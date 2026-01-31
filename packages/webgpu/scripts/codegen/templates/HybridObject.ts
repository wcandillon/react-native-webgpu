import type { InterfaceDeclaration } from "ts-morph";
import _ from "lodash";

import { resolveType } from "../Descriptors";
import {
  resolveCtor,
  resolveExtra,
  resolveExtraDeps,
  resolveMethod,
  resolveNative,
} from "../model/dawn";

import { mergeParentInterfaces } from "./common";

const instanceAliases: Record<string, string> = {
  GPU: "Instance",
  GPUDeviceLostInfo: "DeviceLostReason",
  GPUSupportedLimits: "Limits",
};

const deprecatedMethods = [
  "requestAdapterInfo",
  // New methods not yet implemented
  "setImmediates",
];
const propblackList = [
  "onuncapturederror",
  "label",
  "prototype",
  // New properties not yet implemented
  "maxStorageBuffersInVertexStage",
  "maxStorageBuffersInFragmentStage",
  "maxStorageTexturesInVertexStage",
  "maxStorageTexturesInFragmentStage",
  "maxImmediateSize",
  "textureBindingViewDimension",
  "adapterInfo",
];

// const propWhiteList: string[] = [
//   //"info"
//   //"label",
// ];

export const getHybridObject = (decl: InterfaceDeclaration) => {
  mergeParentInterfaces(decl);
  const className = decl.getName();
  const dependencies = new Set<string>();
  dependencies.add("string");
  const extraDeps = resolveExtraDeps(className);
  extraDeps.forEach((d) => dependencies.add(d));
  const properties = decl
    .getProperties()
    .filter(
      (m) =>
        !m.getName().startsWith("__") && !propblackList.includes(m.getName()),
    )
    .map((signature) => {
      const nativeMethod = resolveNative(
        className,
        `get${_.upperFirst(signature.getName())}`,
      );
      const type = resolveType(signature.getType(), {
        signature,
        dependencies,
        typeNode: signature.getTypeNode(),
        className,
        name: signature.getName(),
        debug: `${className}.${signature.getName()}`,
        native: nativeMethod ? nativeMethod?.returns : undefined,
      });
      return { type, name: signature.getName() };
    });
  const seenMethods = new Set<string>();
  const methods = decl
    .getMethods()
    .filter((m) => !deprecatedMethods.includes(m.getName()))
    .map((signature) => {
      const methodName = signature.getName();
      // Skip if we've already processed this method (handles overloads)
      if (seenMethods.has(methodName)) {
        return null;
      }
      seenMethods.add(methodName);

      const resolved = resolveMethod(className, methodName);
      if (resolved) {
        resolved.deps.forEach((dep) => {
          dependencies.add(dep);
        });
        return {
          name: methodName,
          ...resolved,
        };
      }
      const nativeMethod = resolveNative(className, methodName);

      const params = signature.getParameters();
      const returnType = resolveType(signature.getReturnType(), {
        signature,
        dependencies,
        typeNode: signature.getReturnTypeNode(),
        className,
        name: methodName,
        debug: `Return value of ${className}.${methodName}`,
        native: nativeMethod ? nativeMethod?.returns : undefined,
      });
      return {
        name: methodName,
        returnType,
        args: params.map((param, i) => ({
          name: param.getName(),
          type: resolveType(param.getType(), {
            signature: param,
            dependencies,
            typeNode: param.getTypeNode(),
            className,
            name: param.getName(),
            debug: `Parameter ${param.getName()} of ${className}.${methodName}`,
            native:
              nativeMethod && nativeMethod?.args && nativeMethod?.args[i]
                ? nativeMethod?.args?.[i].type
                : undefined,
          }),
        })),
      };
    })
    .filter((m) => m !== null);
  const hasLabel = decl.getProperty("label") !== undefined;
  const instanceName = `wgpu::${instanceAliases[className] || className.substring(3)}`;
  const ctor = resolveCtor(className);
  const needsAsync =
    decl
      .getMethods()
      .filter((m) => m.getReturnType().getSymbol()?.getName() === "Promise")
      .length > 0;
  const ctorParams: { name: string; type: string }[] = [
    { name: "instance", type: instanceName },
  ];
  if (needsAsync) {
    ctorParams.push({
      name: "async",
      type: "std::shared_ptr<async::AsyncRunner>",
    });
    dependencies.add("memory");
  }
  if (hasLabel) {
    ctorParams.push({ name: "label", type: "std::string" });
  }
  const asyncIncludes = needsAsync
    ? `
#include "rnwgpu/async/AsyncRunner.h"
#include "rnwgpu/async/AsyncTaskHandle.h"`
    : "";
  return `#pragma once

${Array.from(dependencies)
  .filter((dep) => dep[0] === dep[0].toLowerCase())
  .map((dep) => `#include <${dep}>`)
  .join("\n")}

#include "Unions.h"

#include "NativeObject.h"
${asyncIncludes}

#include "webgpu/webgpu_cpp.h"

${Array.from(dependencies)
  .filter((dep) => dep[0] !== dep[0].toLowerCase())
  .map((dep) => `#include "${dep}.h"`)
  .join("\n")}

namespace rnwgpu {

namespace jsi = facebook::jsi;

class ${className} : public NativeObject<${className}> {
public:
  static constexpr const char *CLASS_NAME = "${className}";

  ${
    ctor
      ? ctor
      : `explicit ${className}(${ctorParams.map((param) => `${param.type} ${param.name}`).join(", ")}) : NativeObject(CLASS_NAME), ${ctorParams.map((param) => `_${param.name}(${param.name})`).join(", ")} {}`
  }

public:
  std::string getBrand() { return CLASS_NAME; }

  ${methods
    .map((method) => {
      const args = method.args
        .map((arg) => `${arg.type} ${arg.name}`)
        .join(", ");
      return `${method.returnType} ${method.name}(${args});`;
    })
    .join("\n")}

  ${properties.map((prop) => `${prop.type} get${_.upperFirst(prop.name)}();`).join("\n")}

  ${
    hasLabel
      ? `std::string getLabel() { return _label; }
  void setLabel(const std::string& label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }`
      : ""
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &${className}::getBrand);
    ${methods
      .map(
        (method) =>
          `installMethod(runtime, prototype, "${method.name}", &${className}::${method.name});`,
      )
      .join("\n    ")}
    ${properties.map((prop) => `installGetter(runtime, prototype, "${prop.name}", &${className}::get${_.upperFirst(prop.name)});`).join("\n    ")}
    ${
      hasLabel
        ? `installGetterSetter(runtime, prototype, "label", &${className}::getLabel, &${className}::setLabel);`
        : ""
    }
  }

  inline const ${instanceName} get() { return _instance; }

  ${resolveExtra(className)}

private:
  ${ctorParams.map((param) => `${param.type} _${param.name};`).join("\n  ")}
};

} // namespace rnwgpu
`;
};
