/* eslint-disable max-len */
import type { InterfaceDeclaration } from "ts-morph";
import _ from "lodash";

import { getJSIMethod } from "./common";

const instanceAliases: Record<string, string> = {
  GPU: "Instance",
};

const whiteList = [
  "requestAdapter",
  "requestDevice",
  "createBuffer",
  "unmap",
  "getMappedRange",
];

export const getHybridObject = (decl: InterfaceDeclaration) => {
  const name = decl.getName();
  const methods = decl
    .getMethods()
    .map((m) => getJSIMethod(m))
    .filter((m) => whiteList.includes(m.name));
  const dependencies = methods.flatMap((method) => method.dependencies);
  const instanceName = `wgpu::${instanceAliases[name] || name.substring(3)}`;
  return `#pragma once

#include <memory>
#include <string>
#include <future>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

${dependencies.map((dep) => `#include "${dep}.h"`).join("\n")}

namespace rnwgpu {

namespace m = margelo;

class ${name} : public m::HybridObject {
public:
  explicit ${name}(std::shared_ptr<${instanceName}> instance) : HybridObject("${name}"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  ${methods
    .filter((method) => method.async)
    .map((method) => {
      return `std::future<std::shared_ptr<${method.returns}>> ${method.name}(${method.args.join(", ")});`;
    })
    .join("\n")}

  ${methods
    .filter((method) => !method.async)
    .map((method) => {
      const isUndefined = method.returns;
      return `${isUndefined ? "void" : `std::shared_ptr<${method.returns}>`} ${method.name}(${method.args.join(", ")}) {
      ${isUndefined ? "" : "auto result = "}_instance->${_.upperFirst(method.name)}(${method.argNames.map((n) => `${n}->getInstance()`).join(", ")});
      ${isUndefined ? "" : "return std::make_shared<${method.returns}>(std::make_shared<${method.wgpuReturns}>(result));"}
    }`;
    })
    .join("\n")}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &${name}::getBrand, this);
    ${methods
      .map(
        (method) =>
          `registerHybridMethod("${method.name}", &${name}::${method.name}, this);`,
      )
      .join("\n")}
  }

private:
  std::shared_ptr<${instanceName}> _instance;
};
} // namespace rnwgpu`;
};
