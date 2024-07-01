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
      return ${wgpuName}::${_.upperFirst(_.camelCase(val))};
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
