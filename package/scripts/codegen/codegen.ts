import * as path from "path";
import { writeFileSync } from "fs";

import type { VariableDeclaration } from "ts-morph";
import { Node, Project } from "ts-morph";

import { $ } from "../build/util";

import { getEnum } from "./templates/Enum";

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
    if (Node.isConstructorDeclaration(child)) {
      found = true;
      return false; // Exit early
    }
    return;
  });

  return found;
};

const hasProptotype = (node: VariableDeclaration) => {
  let found = false;

  node.getDescendants().forEach((child) => {
    if (Node.isPropertySignature(child) && child.getName() === "prototype") {
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
      !hasConstructor(decl) &&
      !hasProptotype(decl),
  )
  .forEach((varDecl) => {
    const file = path.resolve(
      __dirname,
      `../../cpp/rnwgpu/api/${varDecl.getName()}.h`,
    );
    $(`touch ${file}`);
    writeFileSync(file, getEnum(varDecl), "utf8");
    console.log(`Generated ${file}`);
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
