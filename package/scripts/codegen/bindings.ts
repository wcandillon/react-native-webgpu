import * as path from "path";

import { Project } from "ts-morph";

// Define the path to the WebGPU type declaration file
const tsConfigFilePath = path.resolve(__dirname, "../../tsconfig.json");
const filePath = path.resolve(
  __dirname,
  "../../node_modules/@webgpu/types/dist/index.d.ts",
);
const project = new Project({
  tsConfigFilePath,
});

// Add source files to the project
const sourceFile = project.addSourceFileAtPath(filePath);

// Inspecting the AST
sourceFile.forEachChild((node) => {
  console.log(`Node kind: ${node.getKindName()}`);
  console.log(`Node text: ${node.getText()}`);
});

// Alternatively, you can navigate through the nodes
const classes = sourceFile.getClasses();
classes.forEach((cls) => {
  console.log(`Class name: ${cls.getName()}`);
  cls.getMethods().forEach((method) => {
    console.log(`Method name: ${method.getName()}`);
  });
});
