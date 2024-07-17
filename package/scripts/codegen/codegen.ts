import * as path from "path";

import type { VariableDeclaration } from "ts-morph";
import { Node, Project } from "ts-morph";

import { getEnum } from "./templates/Enum";
import { writeFile } from "./util";
import { getHybridObject } from "./templates/HybridObject";
import type { Union } from "./templates/Unions";
import { Unions } from "./templates/Unions";
import { getDescriptor } from "./Descriptors";

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

const unions: Union[] = [];

// Unions
console.log("===");
console.log("Unions");
console.log("===");
sourceFile
  .getTypeAliases()
  .filter((typeAlias) => {
    const type = typeAlias.getType();
    return type.isUnion() && type.getUnionTypes()[0].isStringLiteral();
  })
  .forEach((typeAlias) => {
    unions.push({
      name: typeAlias.getName(),
      values: typeAlias
        .getType()
        .getUnionTypes()
        .map((u) => u.getText().replace(/"/g, "")),
    });
  });

writeFile("union", "Unions", Unions(unions));

// Enums
console.log("===");
console.log("Enums");
console.log("===");
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
    writeFile("enum", varDecl.getName(), getEnum(varDecl));
  });

// Errors
console.log("===");
console.log("Errors");
console.log("===");
sourceFile
  .getVariableDeclarations()
  .filter(
    (decl) =>
      decl.getName().startsWith("GPU") && decl.getName().endsWith("Error"),
  )
  .forEach((variableDeclaration) => {
    console.log(`Error class not generated: ${variableDeclaration.getName()}`);
  });

// Objects
console.log("===");
console.log("Objects");
console.log("===");
const hybridObject = sourceFile
  .getInterfaces()
  .filter(
    (decl) =>
      decl.getName().startsWith("GPU") &&
      !decl.getName().endsWith("Mixin") &&
      !decl.getName().endsWith("Error") &&
      !decl.getName().endsWith("Base") &&
      decl.getProperty("__brand") !== undefined,
  );
hybridObject.forEach((decl) => {
  writeFile("object", decl.getName(), getHybridObject(decl));
});

// Descriptors
// the following two descriptors map to:
// type GPUCommandBufferDescriptor =
//   GPUObjectDescriptorBase;
// type GPUCommandEncoderDescriptor =
//   GPUObjectDescriptorBase;
const GPUCommandBufferDescriptor = sourceFile.addInterface({
  name: "GPUCommandBufferDescriptor",
  isExported: true,
});
const GPUCommandEncoderDescriptor = sourceFile.addInterface({
  name: "GPUCommandEncoderDescriptor",
  isExported: true,
});
GPUCommandEncoderDescriptor.addExtends("GPUObjectDescriptorBase");
GPUCommandBufferDescriptor.addExtends("GPUObjectDescriptorBase");

/*
type GPUQueueDescriptor =
  GPUObjectDescriptorBase;
type GPURenderBundleDescriptor =
  GPUObjectDescriptorBase;
  */
const GPUQueueDescriptor = sourceFile.addInterface({
  name: "GPUQueueDescriptor",
  isExported: true,
});
const GPURenderBundleDescriptor = sourceFile.addInterface({
  name: "GPURenderBundleDescriptor",
  isExported: true,
});
GPUQueueDescriptor.addExtends("GPUObjectDescriptorBase");
GPURenderBundleDescriptor.addExtends("GPUObjectDescriptorBase");

console.log("===");
console.log("Descriptors");
console.log("===");
const toSkip = [
  "GPUOrigin2DDictStrict",
  "GPUExtent3DDictStrict",
  "GPUExtent3DDict",
  "GPUOrigin2DDict",
  "GPUOrigin3DDict",
];
sourceFile
  .getInterfaces()
  .filter(
    (decl) =>
      decl.getName().startsWith("GPU") &&
      !decl.getName().endsWith("Mixin") &&
      !decl.getName().endsWith("Error") &&
      !decl.getName().endsWith("Base") &&
      !toSkip.includes(decl.getName()) &&
      decl.getProperty("__brand") === undefined,
  )
  .forEach((decl) => {
    writeFile(
      "descriptor",
      decl.getName(),
      getDescriptor(
        decl,
        //   unions,
        // hybridObject.map((d) => d.getName()),
      ),
    );
  });
