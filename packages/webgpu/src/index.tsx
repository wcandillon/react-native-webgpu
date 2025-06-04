import "@webgpu/types";

export * from "./main";

declare global {
  interface Navigator {
    gpu: GPU;
  }
}
