/* eslint-disable max-len */
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
};

const deprecatedMethods = ["requestAdapterInfo"];
const propblackList = ["onuncapturederror", "label", "prototype"];

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
  const methods = decl
    .getMethods()
    .filter((m) => !deprecatedMethods.includes(m.getName()))
    .map((signature) => {
      const resolved = resolveMethod(className, signature.getName());
      if (resolved) {
        resolved.deps.forEach((dep) => {
          dependencies.add(dep);
        });
        return {
          name: signature.getName(),
          ...resolved,
        };
      }
      const nativeMethod = resolveNative(className, signature.getName());

      const params = signature.getParameters();
      const returnType = resolveType(signature.getReturnType(), {
        signature,
        dependencies,
        typeNode: signature.getReturnTypeNode(),
        className,
        name: signature.getName(),
        debug: `Return value of ${className}.${signature.getName()}`,
        native: nativeMethod ? nativeMethod?.returns : undefined,
      });
      return {
        name: signature.getName(),
        returnType,
        args: params.map((param, i) => ({
          name: param.getName(),
          type: resolveType(param.getType(), {
            signature: param,
            dependencies,
            typeNode: param.getTypeNode(),
            className,
            name: param.getName(),
            debug: `Parameter ${param.getName()} of ${className}.${signature.getName()}`,
            native:
              nativeMethod && nativeMethod?.args && nativeMethod?.args[i]
                ? nativeMethod?.args?.[i].type
                : undefined,
          }),
        })),
      };
    });
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
    ctorParams.push({ name: "async", type: "std::shared_ptr<AsyncRunner>" });
    dependencies.add("memory");
  }
  if (hasLabel) {
    ctorParams.push({ name: "label", type: "std::string" });
  }
  return `#pragma once

${Array.from(dependencies)
  .filter((dep) => dep[0] === dep[0].toLowerCase())
  .map((dep) => `#include <${dep}>`)
  .join("\n")}

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

${Array.from(dependencies)
  .filter((dep) => dep[0] !== dep[0].toLowerCase())
  .map((dep) => `#include "${dep}.h"`)
  .join("\n")}

namespace rnwgpu {

namespace m = margelo;

class ${className} : public m::HybridObject {
public:
  ${
    ctor
      ? ctor
      : `  explicit ${className}(${ctorParams.map((param) => `${param.type} ${param.name}`).join(", ")}) : HybridObject("${className}"), ${ctorParams.map((param) => `_${param.name}(${param.name})`).join(", ")} {}`
  }

public:
  std::string getBrand() { return _name; }

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
    void setLabel(const std::string& label) { _label = label;
      _instance.SetLabel(_label.c_str());
    }`
      : ""
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &${className}::getBrand, this);
    ${methods
      .map(
        (method) =>
          `registerHybridMethod("${method.name}", &${className}::${method.name}, this);`,
      )
      .join("\n")}
    ${properties.map((prop) => `registerHybridGetter("${prop.name}", &${className}::get${_.upperFirst(prop.name)}, this);`).join("\n")}
    ${
      hasLabel
        ? `registerHybridGetter("label", &${className}::getLabel, this);
      registerHybridSetter("label", &${className}::setLabel, this);`
        : ""
    }
  }
  
  inline const ${instanceName} get() {
    return _instance;
  }

 private:
  ${ctorParams.map((param) => `${param.type} _${param.name};`).join("\n")}
  ${resolveExtra(className)}
};

} // namespace rnwgpu`;
};
