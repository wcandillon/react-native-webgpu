import * as fs from "fs";
import * as path from "path";

import * as ts from "@typescript-eslint/typescript-estree";

// Define the path to the WebGPU type declaration file
const filePath = path.resolve(
  __dirname,
  "../../node_modules/@webgpu/types/dist/index.d.ts",
);

// Read the content of the file
const fileContent = fs.readFileSync(filePath, "utf-8");

// Parse the content to an AST
const ast = ts.parse(fileContent, {
  loc: true,
  range: true,
  tokens: true,
  comment: true,
  jsx: false,
  useJSXTextNode: false,
  loggerFn: false,
  errorOnUnknownASTType: false,
  errorOnTypeScriptSyntacticAndSemanticIssues: false,
  preserveNodeMaps: true,
});

// Log the AST to the console
console.log(JSON.stringify(ast, null, 2));
