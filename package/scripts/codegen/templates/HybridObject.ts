/* eslint-disable max-len */
import type { InterfaceDeclaration } from "ts-morph";
import _ from "lodash";

import {
  getJSIMethod,
  getJSIProp,
  mergeParentInterfaces,
  wrapType,
} from "./common";

const instanceAliases: Record<string, string> = {
  GPU: "Instance",
};

const methodWhiteList = [
  "requestAdapter",
  "requestDevice",
  "createBuffer",
  "unmap",
  "getMappedRange",
];

const propWhiteList: string[] = [
  //"info"
  //"label",
];

export const getHybridObject = (decl: InterfaceDeclaration) => {
  mergeParentInterfaces(decl);
  const name = decl.getName();
  const methods = decl
    .getMethods()
    .map((m) => getJSIMethod(name, m))
    .filter((m) => methodWhiteList.includes(m.name));
  const properties = decl
    .getProperties()
    .filter(
      (m) =>
        !m.getName().startsWith("__") && propWhiteList.includes(m.getName()),
    )
    .map((p) => getJSIProp(p));
  const hasLabel = decl.getProperty("label") !== undefined;
  const dependencies = [
    ...methods.flatMap((method) => method.dependencies),
    ...properties.flatMap((prop) => prop.dependencies),
  ];
  const instanceName = `wgpu::${instanceAliases[name] || name.substring(3)}`;
  const labelCtrArg = hasLabel ? ", std::string label" : "";
  const labelCtrInit = hasLabel ? ", _label(label)" : "";
  const getLabel = hasLabel
    ? 'std::string label = aDescriptor->label ? std::string(aDescriptor->label) : "";'
    : "";
  return `#pragma once

#include <memory>
#include <string>
#include <future>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

${dependencies.map((dep) => `#include "${dep}.h"`).join("\n")}

namespace rnwgpu {

namespace m = margelo;

class ${name} : public m::HybridObject {
public:
  explicit ${name}(std::shared_ptr<${instanceName}> instance${labelCtrArg}) : HybridObject("${name}"), _instance(instance)${labelCtrInit} {}

public:
  std::string getBrand() { return _name; }

  ${methods
    .filter((method) => method.async)
    .map((method) => {
      return `std::future<${wrapType(method.returns)}> ${method.name}(${method.args.map((a) => `${wrapType(a.type)} ${a.name}`).join(", ")});`;
    })
    .join("\n")}

  ${methods
    .filter((method) => !method.async)
    .map((method) => {
      const isUndefined = method.returns === "undefined";
      const returnType = isUndefined ? "void" : wrapType(method.returns);
      const args = method.args
        .map((a) => `${wrapType(a.type, a.optional)} ${a.name}`)
        .join(", ");
      let returnValue = isUndefined
        ? ""
        : `return std::make_shared<${method.returns}>(std::make_shared<${method.wgpuReturns}>(result)${hasLabel ? ", label" : ""});`;
      if (method.returns === "MutableJSIBuffer") {
        returnValue =
          "return std::make_shared<MutableJSIBuffer>(result, _instance->GetSize());";
      }
      return `${returnType} ${method.name}(${args}) {
      ${method.args
        .map((arg) => {
          return `auto a${_.upperFirst(arg.name)} = ${arg.optional ? `${arg.name}.value_or(${arg.defaultValue})` : `${arg.name}->getInstance()`};`;
        })
        .join("\n")}
      ${getLabel}
      ${isUndefined ? "" : "auto result = "}_instance->${_.upperFirst(method.name)}(${method.argNames.map((n) => `a${_.upperFirst(n)}`).join(", ")});
      ${returnValue}
    }`;
    })
    .join("\n")}

  ${properties.map((prop) => `std::shared_ptr<${prop.type}> get${_.upperFirst(prop.name)}() {}`).join("\n")}

  ${hasLabel ? "std::string getLabel() { return _label; }" : ""}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &${name}::getBrand, this);
    ${methods
      .map(
        (method) =>
          `registerHybridMethod("${method.name}", &${name}::${method.name}, this);`,
      )
      .join("\n")}
    ${properties.map((prop) => `registerHybridGetter("${prop.name}", &${name}::get${_.upperFirst(prop.name)}, this);`).join("\n")}
    ${hasLabel ? `registerHybridGetter("label", &${name}::getLabel, this);` : ""}
  }

private:
  std::shared_ptr<${instanceName}> _instance;
  ${hasLabel ? "std::string _label;" : ""}
};
} // namespace rnwgpu`;
};
