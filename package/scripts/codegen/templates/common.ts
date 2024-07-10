import _ from "lodash";
import type { InterfaceDeclaration } from "ts-morph";
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
      for (const prop of mergedParentInterface.getProperties()) {
        if (!interfaceDecl.getProperty(prop.getName())) {
          interfaceDecl.addProperty(prop.getStructure());
        }
      }

      // Merge methods from parent to child
      for (const method of mergedParentInterface.getMethods()) {
        if (!interfaceDecl.getMethod(method.getName())) {
          interfaceDecl.addMethod(method.getStructure());
        }
      }
    }
  }

  return interfaceDecl;
};
