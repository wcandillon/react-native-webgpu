/* eslint-disable max-len */
import type { InterfaceDeclaration } from "ts-morph";
import _ from "lodash";

import { resolveType } from "../Descriptors";
import { resolveCtor, resolveExtra } from "../model/dawn";

import { mergeParentInterfaces } from "./common";

const instanceAliases: Record<string, string> = {
  GPU: "Instance",
};

const methodWhiteList = [
  // GPU
  "getPreferredCanvasFormat",
  // Texture
  "createTexture",
  // Queue
  "writeBuffer",
  "submit",
  //
  "requestAdapter",
  "requestDevice",
  // Device
  "createBuffer",
  "createCommandEncoder",
  "createShaderModule",
  "createRenderPipeline",
  "destroy",
  "createTexture",
  "createSampler",
  "createView",
  "createBindGroup",
  // Buffer
  "unmap",
  "getMappedRange",
  "mapAsync",
  // CommandEncoder,
  "copyBufferToBuffer",
  "finish",
  "beginRenderPass",
  "setPipeline",
  "draw",
  "end",
  "getBindGroupLayout",
  "setBindGroup",
  "copyTextureToBuffer",
  "createComputePipeline",
  "beginComputePass",
  "dispatchWorkgroups",
];

const propWhiteList: Record<string, string[]> = {
  GPUBuffer: ["size", "usage", "mapState"],
  GPUDevice: ["queue"],
  GPUTexture: [
    "width",
    "height",
    "depthOrArrayLayers",
    "mipLevelCount",
    "sampleCount",
    "dimension",
    "format",
    "usage",
  ],
};

// const propWhiteList: string[] = [
//   //"info"
//   //"label",
// ];

export const getHybridObject = (decl: InterfaceDeclaration) => {
  mergeParentInterfaces(decl);
  const className = decl.getName();
  const dependencies = new Set<string>();
  const properties = decl
    .getProperties()
    .filter(
      (m) =>
        !m.getName().startsWith("__") &&
        propWhiteList[className] &&
        propWhiteList[className].includes(m.getName()),
    )
    .map((signature) => {
      const type = resolveType(signature.getType(), {
        signature,
        dependencies,
        typeNode: signature.getTypeNode(),
        className,
        name: signature.getName(),
      });
      return { type, name: signature.getName() };
    });
  const methods = decl
    .getMethods()
    .filter((m) => methodWhiteList.includes(m.getName()))
    .map((signature) => {
      const params = signature.getParameters();
      const returnType = resolveType(signature.getReturnType(), {
        signature,
        dependencies,
        typeNode: signature.getReturnTypeNode(),
        className,
        name: signature.getName(),
      });
      return {
        name: signature.getName(),
        returnType: returnType,
        args: params.map((param) => ({
          name: param.getName(),
          type: resolveType(param.getType(), {
            signature: param,
            dependencies,
            typeNode: param.getTypeNode(),
            className,
            name: param.getName(),
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
  }
  if (hasLabel) {
    ctorParams.push({ name: "label", type: "std::string" });
  }
  return `#pragma once

#include <memory>
#include <string>
#include <future>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"
#include "ArrayBuffer.h"

#include "webgpu/webgpu_cpp.h"

${Array.from(dependencies)
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

  ${hasLabel ? "std::string getLabel() { return _label; }" : ""}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &${className}::getBrand, this);
    ${methods
      .map(
        (method) =>
          `registerHybridMethod("${method.name}", &${className}::${method.name}, this);`,
      )
      .join("\n")}
    ${properties.map((prop) => `registerHybridGetter("${prop.name}", &${className}::get${_.upperFirst(prop.name)}, this);`).join("\n")}
    ${hasLabel ? `registerHybridGetter("label", &${className}::getLabel, this);` : ""}
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
