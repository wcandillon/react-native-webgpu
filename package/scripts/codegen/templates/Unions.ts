import _ from "lodash";

import { externalDefs } from "../Descriptors";

export interface Union {
  name: string;
  values: string[];
}

export const Unions = (unions: Union[]) => {
  return `#pragma once

#include <string>

#include "webgpu/webgpu_cpp.h"
#include "External.h"
#include "RNFEnumMapper.h"

namespace margelo {
namespace EnumMapper {

${unions
  .filter(
    (union) =>
      union.name !== "GPUCanvasAlphaMode" &&
      union.name !== "GPUPipelineErrorReason",
  )
  .map((union) => Union(union))
  .join("\n")}
} // namespace EnumMapper
} // namespace margelo
`;
};

const enumName = (input: string) => {
  const map: Record<string, string> = {
    "1d": "e1D",
    "2d": "e2D",
    "3d": "e3D",
    "2d-array": "e2DArray",
    "unorm10-10-10-2": "Unorm10_10_10_2",
  };
  if (map[input] !== undefined) {
    return map[input];
  }
  // Convert kebab-case to CamelCase
  let result = _.camelCase(input);

  // Define special abbreviation and color formats to be fully capitalized
  const specialAbbreviations = ["cpu", "gpu", "ccw", "cw"];
  const colorFormats = ["astc", "eac", "etc2", "rgba", "bc", "bgra", "rg"];

  // Split the string to handle individual words for special cases
  let parts = result.split(/(?=[A-Z])/);

  // Process each part for special cases
  parts = parts.map((part) => {
    const lowerPart = part.toLowerCase();
    if (specialAbbreviations.some((format) => lowerPart.includes(format))) {
      return lowerPart.toUpperCase();
    } else if (colorFormats.some((format) => lowerPart.startsWith(format))) {
      return lowerPart.toUpperCase();
    }
    return part;
  });

  // Join all parts and capitalize the first character
  result = parts.join("").replace(/(\d)X(\d)/g, "$1x$2");
  result = result.charAt(0).toUpperCase() + result.slice(1);
  // Handle edge case: Strings starting with a number
  return result;
};

const Union = (union: Union) => {
  const { name } = union;
  const wgpuName = externalDefs.includes(name)
    ? `rnwgpu::${name}`
    : `wgpu::${name.substring(3)}`;
  return `
static void convertJSUnionToEnum(const std::string& inUnion, ${wgpuName}* outEnum) { 
    ${union.values
      .map((val, index) => {
        return `${index > 0 ? "else" : ""} if (inUnion == "${val}") {
      *outEnum = ${wgpuName}::${enumName(val)};
    }`;
      })
      .join("\n")}
    else {
      throw invalidUnion(inUnion);
    }
  }

  static void convertEnumToJSUnion(${wgpuName} inEnum, std::string* outUnion) {
    switch (inEnum) {
      ${union.values
        .map((val) => {
          return `case ${wgpuName}::${enumName(val)}:
          *outUnion = "${val}";
          break;`;
        })
        .join("\n")}
      default:
        throw invalidEnum(inEnum);
    }
  }
`;
};
