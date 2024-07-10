import * as path from "path";
import exp from "constants";

import type { SourceFile } from "ts-morph";
import { Project } from "ts-morph";

import { resolveMethod } from "../model";

let sourceFile: SourceFile;

beforeAll(async () => {
  const tsConfigFilePath = path.resolve(__dirname, "../../../../tsconfig.json");
  const filePath = path.resolve(
    __dirname,
    "../../../../node_modules/@webgpu/types/dist/index.d.ts",
  );
  const project = new Project({
    tsConfigFilePath,
  });

  sourceFile = project.addSourceFileAtPath(filePath);
});

const getMethodSignature = (className: string, methodName: string) => {
  const classDecl = sourceFile.getInterface(className);
  if (!classDecl) {
    throw new Error(`Class ${className} not found`);
  }
  const methodDecl = classDecl.getMethod(methodName);
  if (!methodDecl) {
    throw new Error(`Method ${methodName} not found`);
  }
  return methodDecl;
};

describe("Model", () => {
  it("GPU", () => {
    let method = resolveMethod(getMethodSignature("GPU", "requestAdapter"));
    expect(method).toBeTruthy();
    expect(method.returns).toBe("std::future<std::shared_ptr<GPUAdapter>>");
    expect(method.args.length).toBe(1);
    expect(method.args[0].type).toEqual(
      "std::shared_ptr<GPURequestAdapterOptions>",
    );
    method = resolveMethod(
      getMethodSignature("GPU", "getPreferredCanvasFormat"),
    );
    expect(method.returns).toBe("wgpu::TextureFormat");
  });
  it("GPUAdapter", () => {
    const method = resolveMethod(
      getMethodSignature("GPUAdapter", "requestDevice"),
    );
    expect(method).toBeTruthy();
    expect(method.returns).toBe("std::future<std::shared_ptr<GPUDevice>>");
    expect(method.args.length).toBe(1);
    expect(method.args[0].type).toEqual("std::shared_ptr<GPUDeviceDescriptor>");
  });
  it("GPUDevice", () => {
    const method = resolveMethod(
      getMethodSignature("GPUDevice", "createBuffer"),
    );
    expect(method.returns).toBe("std::shared_ptr<GPUBuffer>");
    expect(method.args.length).toBe(1);
    expect(method.args[0].type).toEqual("std::shared_ptr<GPUBufferDescriptor>");
  });
  it("GPUBuffer", () => {
    let method = resolveMethod(getMethodSignature("GPUBuffer", "unmap"));
    expect(method.returns).toBe("void");
    expect(method.args.length).toBe(0);

    method = resolveMethod(getMethodSignature("GPUBuffer", "getMappedRange"));
    expect(method.returns).toBe("std::shared_ptr<MutableJSIBuffer>");
    expect(method.args.length).toBe(2);
    expect(method.args[0].type).toEqual("std::optional<size_t>");
    expect(method.args[1].type).toEqual("std::optional<size_t>");
  });
});
