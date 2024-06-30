import type { InterfaceDeclaration } from "ts-morph";

import { getJSIMethod } from "./common";

const instanceAliases: Record<string, string> = {
  GPU: "Instance",
};

export const getHybridObject = (decl: InterfaceDeclaration) => {
  const name = decl.getName();
  const methods = decl
    .getMethods()
    .map((m) => getJSIMethod(m))
    .filter((m) => m.name === "requestAdapter");
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
      return `std::shared_ptr<${method.returns}> ${method.name}(${method.args.join(", ")}) {
      return _instance->${method.name}();
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
