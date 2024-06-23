import * as path from "path";

import type { VariableDeclaration } from "ts-morph";
import { Project, SyntaxKind } from "ts-morph";

// Define the path to the WebGPU type declaration file
const tsConfigFilePath = path.resolve(__dirname, "../../tsconfig.json");
const filePath = path.resolve(
  __dirname,
  "../../node_modules/@webgpu/types/dist/index.d.ts",
);
const project = new Project({
  tsConfigFilePath,
});

const sourceFile = project.addSourceFileAtPath(filePath);

const hasConstructor = (node: VariableDeclaration) => {
  let found = false;

  node.getDescendants().forEach((child) => {
    if (child.getKind() === SyntaxKind.ConstructSignature) {
      found = true;
      return false; // Exit early
    }
    return;
  });

  return found;
};
// Enums
sourceFile
  .getVariableDeclarations()
  .filter(
    (decl) =>
      decl.getName().startsWith("GPU") &&
      !decl.getName().endsWith("Error") &&
      !hasConstructor(decl),
  )
  .forEach((variableDeclaration) => {
    console.log(`Enum name: ${variableDeclaration.getName()}`);
  });
// Errors
sourceFile
  .getVariableDeclarations()
  .filter(
    (decl) =>
      decl.getName().startsWith("GPU") && decl.getName().endsWith("Error"),
  )
  .forEach((variableDeclaration) => {
    console.log(`Error name: ${variableDeclaration.getName()}`);
  });
// Objects
sourceFile
  .getInterfaces()
  .filter(
    (decl) =>
      decl.getName().startsWith("GPU") && !decl.getName().endsWith("Mixin"),
  )
  .forEach((interfaceDeclaration) => {
    const hasMethods = interfaceDeclaration.getMethods().length > 0;
    if (hasMethods) {
      console.log(`Object name: ${interfaceDeclaration.getName()}`);
    }
  });

// // Inspecting the AST
// sourceFile.forEachChild((node) => {
//   console.log(`Node kind: ${node.getKindName()}`);
//   if (node.getKindName() === "InterfaceDeclaration") {
//     console.log(`Node text: ${node.getText()}`);
//   }
//   //  / console.log(`Node text: ${node.getText()}`);
// });

// // Alternatively, you can navigate through the nodes
// const classes = sourceFile.getClasses();
// classes.forEach((cls) => {
//   console.log(`Class name: ${cls.getName()}`);
//   cls.getMethods().forEach((method) => {
//     console.log(`Method name: ${method.getName()}`);
//   });
// });
