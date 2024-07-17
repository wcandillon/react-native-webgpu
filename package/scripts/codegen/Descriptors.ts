import { InterfaceDeclaration, Type } from "ts-morph";
// import { dawn, mapKeys, hasPropery } from "./model/dawn";
import { Union } from "./templates/Unions";
import { mergeParentInterfaces } from "./templates/common";

// export const generateDescriptors = () => {
//   return mapKeys(dawn)
//     .map((key) => {
//       const value = dawn[key];
//       if (
//         hasPropery(value, "category") &&
//         value.category === "structure" &&
//         !hasPropery(value, "tags")
//       ) {
//         return key;
//       }
//       return;
//     })
//     .filter((t) => t !== undefined);
// };

const resolveType = (type: Type, dependencies: Set<string>): string => {
  if (type.isString()) {
    dependencies.add("string");
    return "std::string";
  } else if (type.isBoolean() || type.isBooleanLiteral()) {
    return "bool";
  } else if (type.isNumber()) {
    return "double";
  } else if (type.isUnion()) {
    const unionTypes = type.getUnionTypes().filter(t => !t.isUndefined());
    if (unionTypes.length === 1) {
      return resolveType(unionTypes[0], dependencies);
    } else {
      const unionNames = Array.from(new Set(unionTypes.map(t => resolveType(t, dependencies))));
      if (unionNames.length === 1) {
        return unionNames[0];
      } else {
        dependencies.add("variant");
        return `std::variant<${unionNames.join(", ")}>`;
      }
    }
  }
  return "unknown";
  //throw new Error("Unhandled type: " + type.getText());
}

export const getDescriptor = (
  decl: InterfaceDeclaration,
  _unions: Union[],
  _hybridObjects: string[],
) => {
  mergeParentInterfaces(decl);
  const name = decl.getName();
  const dependencies = new Set<string>();
  // we filter sourceMap?: any; for now
  const props = decl.getProperties().filter(p => !p.getType().isAny()).map((prop) => {
    const mandatoryType = resolveType(prop.getType(), dependencies);
    const type = prop.hasQuestionToken()? `std::optional<${mandatoryType}>` : mandatoryType;
    if (prop.hasQuestionToken()) {
      dependencies.add("optional");
    }
    const debug = prop.getTypeNode()?.getText() ?? "";
    return {
      name: prop.getName(),
      type,
      debug
    };
  });
  return `#pragma once

${Array.from(dependencies).map(dep => `#include ${dep[0].toLowerCase() === dep[0] ? `<${dep}>` : `"${dep}.h"`}`).join("\n")}

namespace rnwgpu {

struct ${name} {
  ${props.map(p => `${p.type} ${p.name}; // ${p.debug.replace(/\n/g, ' ')}`).join("\n  ")}
};

} // namespace rnwgpu`;
};
