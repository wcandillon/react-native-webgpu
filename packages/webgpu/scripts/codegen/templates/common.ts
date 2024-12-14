import _ from "lodash";
import type { InterfaceDeclaration, Type } from "ts-morph";
import { SyntaxKind } from "ts-morph";

export const mergeParentInterfaces = (interfaceDecl: InterfaceDeclaration) => {
  if (interfaceDecl.getKind() !== SyntaxKind.InterfaceDeclaration) {
    return interfaceDecl;
  }
  const parentInterfaces = interfaceDecl.getBaseDeclarations();
  for (const parentInterface of parentInterfaces) {
    const parentDeclaration = parentInterface
      .getType()
      .getSymbol()
      ?.getDeclarations()[0];

    if (parentDeclaration) {
      const parentInterfaceDecl = parentDeclaration as InterfaceDeclaration;

      // Recursively merge parent interfaces
      const mergedParentInterface = mergeParentInterfaces(parentInterfaceDecl);

      // Merge properties from parent to child
      if (
        mergedParentInterface &&
        typeof mergedParentInterface.getProperties === "function"
      ) {
        const properties = mergedParentInterface.getProperties();
        for (const prop of properties) {
          if (!interfaceDecl.getProperty(prop.getName())) {
            interfaceDecl.addProperty(prop.getStructure());
          }
        }
      }
      if (
        mergedParentInterface &&
        typeof mergedParentInterface.getMethods === "function"
      ) {
        // Merge methods from parent to child
        for (const method of mergedParentInterface.getMethods()) {
          if (!interfaceDecl.getMethod(method.getName())) {
            interfaceDecl.addMethod(method.getStructure());
          }
        }
      }
    }
  }

  return interfaceDecl;
};

interface DebugType {
  text: string;
  aliasSymbol?: string;
  aliasTypeArguments?: DebugType[];
  symbol?: string;
  typeArguments?: DebugType[];
  unionTypes?: DebugType[];
  intersectionTypes?: DebugType[];
  isAnonymous?: boolean;
  isAny?: boolean;
  isUnknown?: boolean;
  isNever?: boolean;
  isVoid?: boolean;
  isLiteral?: boolean;
  isBoolean?: boolean;
  isNull?: boolean;
  isUndefined?: boolean;
  isNumber?: boolean;
  isString?: boolean;
  isUnion?: boolean;
  isEnum?: boolean;
  isObject?: boolean;
  isClass?: boolean;
  isInterface?: boolean;
  isTuple?: boolean;
  isArray?: boolean;
  isStringLiteral?: boolean;
}

export const debugType = (type: Type): DebugType => {
  return {
    text: type.getText(),
    aliasSymbol: type.getAliasSymbol()?.getName(),
    aliasTypeArguments: type.getAliasTypeArguments().map((t) => debugType(t)),
    symbol: type.getSymbol()?.getName(),
    typeArguments: type.getTypeArguments().map((t) => debugType(t)),
    unionTypes: type.isUnion()
      ? type.getUnionTypes().map((t) => debugType(t))
      : undefined,
    intersectionTypes: type.isIntersection()
      ? type.getIntersectionTypes().map((t) => debugType(t))
      : undefined,
    isStringLiteral: type.isStringLiteral(),
    // isAnonymous: type.isAnonymous(),
    // isAny: type.isAny(),
    // isUnknown: type.isUnknown(),
    // isNever: type.isNever(),
    // isVoid: type.isVoid(),
    // isNull: type.isNull(),
    // isUndefined: type.isUndefined(),
    // isBoolean: type.isBoolean(),
    // isString: type.isString(),
    // isNumber: type.isNumber(),
    // isLiteral: type.isLiteral(),
    // isEnum: type.isEnum(),
    // isObject: type.isObject(),
    // isClass: type.isClass(),
    // isInterface: type.isInterface(),
    // isTuple: type.isTuple(),
    // isArray: type.isArray(),
  };
};
