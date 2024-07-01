import _ from "lodash";

export interface Union {
  name: string;
  values: string[];
}

export const Unions = (unions: Union[]) => {
  return `#pragma once

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"

namespace margelo {

${unions.map((union) => Union(union)).join("\n")}

} // namespace margelo
`;
};

const enumName = (input: string) => {
  const map: Record<string, string> = {
    "1d": "e1D",
    "2d": "e2D",
    "3d": "e3D",
    "2d-array": "e2DArray",
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
  const wgpuName = `wgpu::${name.substring(3)}`;
  return `// Object <> Object
template <>
struct JSIConverter<${wgpuName}> {
  static ${wgpuName}
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    ${union.values
      .map((val) => {
        return `if (str == "${val}") {
      return ${wgpuName}::${enumName(val)};
    }`;
      })
      .join("\n")}
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        ${wgpuName} arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
`;
};
