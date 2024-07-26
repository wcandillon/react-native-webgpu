import WebGPUNativeModule from "./WebGPUNativeModule";

export * from "./Canvas";
export * from "./WebGPUViewNativeComponent";
export { default as WebGPUModule } from "./WebGPUNativeModule";

declare global {
  // eslint-disable-next-line no-var
  var gpu: GPU;
  // eslint-disable-next-line no-var
  var __WebGPUContextRegistry: Record<number, GPUCanvasContext>;
}

WebGPUNativeModule.install();
export const { gpu } = global;
