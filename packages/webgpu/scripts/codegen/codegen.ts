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
  "../../../../node_modules/@webgpu/types/dist/index.d.ts",
);
const project = new Project({
  tsConfigFilePath,
});

const sourceFile = project.addSourceFileAtPath(filePath);

sourceFile.getTypeAlias("GPUColor")!.remove();
sourceFile.addInterface({ name: "GPUColor", isExported: true });
// Descriptors
// the following two descriptors map to:
// type GPUCommandBufferDescriptor =
//   GPUObjectDescriptorBase;
// type GPUCommandEncoderDescriptor =
//   GPUObjectDescriptorBase;
sourceFile.getTypeAlias("GPUCommandBufferDescriptor")!.remove();
sourceFile.getTypeAlias("GPUCommandEncoderDescriptor")!.remove();
//sourceFile.getType("GPUCommandBufferDescriptor");
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
sourceFile.getTypeAlias("GPUQueueDescriptor")!.remove();
sourceFile.getTypeAlias("GPURenderBundleDescriptor")!.remove();
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

/*
// type PredefinedColorSpace = "display-p3" | "srgb";
// type PremultiplyAlpha = "default" | "none" | "premultiply";
*/
// Add PredefinedColorSpace type alias
sourceFile.addTypeAlias({
  name: "PredefinedColorSpace",
  type: '"display-p3" | "srgb"',
  isExported: true,
});

// Add PremultiplyAlpha type alias
sourceFile.addTypeAlias({
  name: "PremultiplyAlpha",
  type: '"default" | "none" | "premultiply"',
  isExported: true,
});

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

// sourceFile.getVariableDeclarations().forEach((decl) => {
//   const [hasNeverConstructor] = decl.getDescendantsOfKind(
//     SyntaxKind.ConstructSignature,
//   );
//   if (hasNeverConstructor && hasNeverConstructor.getReturnType().isNever()) {
//     const name = decl.getName();
//     console.log(`${name}: typeof ${name};`);
//     console.log(`const ${name}: any = {};
//     ${name}[Symbol.hasInstance] = function (instance: object) {
//       return "__brand" in instance && instance.__brand === "${name}";
//     };`);
//   }
// });

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
// the following objects have been manually written for simplicity and therefore can be skipped
const objectsToSkip = [
  "GPUUncapturedErrorEvent",
  "GPUUncapturedErrorEventInit",
  "GPUAdapterInfo",
  "GPUCanvasContext",
  "GPUCompilationInfo",
  "GPUCompilationMessage",
  "GPUDeviceLostInfo",
];
const hybridObject = sourceFile
  .getInterfaces()
  .filter(
    (decl) =>
      decl.getName().startsWith("GPU") &&
      !decl.getName().endsWith("Mixin") &&
      !decl.getName().endsWith("Error") &&
      !decl.getName().endsWith("Base") &&
      !objectsToSkip.includes(decl.getName()) &&
      decl.getProperty("__brand") !== undefined,
  );
hybridObject.forEach((decl) => {
  writeFile("object", decl.getName(), getHybridObject(decl));
});

console.log("===");
console.log("Descriptors");
console.log("===");
const toSkip = [
  "GPUOrigin2DDictStrict",
  "GPUExtent3DDictStrict",
  "GPUExtent3DDict",
  "GPUOrigin2DDict",
  "GPUOrigin3DDict",
  "GPUColorDict",
  "GPUColor",
  // TODO: remove these
  "GPUImageCopyExternalImage",
  "GPURenderPassLayout",
  "GPUExternalTextureDescriptor",
  "GPUBindGroupEntry",
  "GPUCanvasConfiguration",
  "GPUPipelineErrorInit",
  "GPUUncapturedErrorEvent",
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
    writeFile("descriptor", decl.getName(), getDescriptor(decl));
  });
