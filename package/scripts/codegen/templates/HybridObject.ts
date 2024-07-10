/* eslint-disable max-len */
import type { InterfaceDeclaration } from "ts-morph";
import _ from "lodash";

import { resolveMethod } from "../model/model";

import { getJSIProp, mergeParentInterfaces, wrapType } from "./common";

const instanceAliases: Record<string, string> = {
  GPU: "Instance",
};

const methodWhiteList = [
  // GPU
  "getPreferredCanvasFormat",
  //
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
    .filter((m) => methodWhiteList.includes(m.getName()))
    .map((m) => resolveMethod(m));
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
  return `#pragma once

#include <memory>
#include <string>
#include <future>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

${dependencies.map((dep) => `#include "${dep}.h"`).join("\n")}

namespace rnwgpu {

namespace m = margelo;

class ${name} : public m::HybridObject {
public:
  explicit ${name}(${instanceName} instance${labelCtrArg}) : HybridObject("${name}"), _instance(instance)${labelCtrInit} {}

public:
  std::string getBrand() { return _name; }


  ${methods
    .map((method) => {
      const isUndefined = method.returns === "undefined";
      const returnType = isUndefined ? "void" : wrapType(method.returns);
      const args = method.args
        .map((arg) => `${arg.type} ${arg.name}`)
        .join(", ");
      return `${returnType} ${method.name}(${args});`;
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
  ${instanceName} _instance;
  ${hasLabel ? "std::string _label;" : ""}
};
} // namespace rnwgpu`;
};
