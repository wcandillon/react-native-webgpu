import WebGPUNativeModule from "./WebGPUNativeModule";

export { WebGPUView } from "./WebGPUView";
export type { WebGPUViewRef } from "./WebGPUView";
export * from "./WebGPUViewNativeComponent";
export { default as WebGPUModule } from "./WebGPUNativeModule";

declare global {
  // eslint-disable-next-line no-var
  var gpu: GPU;
  var __WebGPUContextRegistry: Record<number, GPUCanvasContext>;
}

global.__WebGPUContextRegistry = {};
export const WebGPUContextRegistry = global.__WebGPUContextRegistry;

WebGPUNativeModule.install();
export const { gpu } = global;

export const mleko = WebGPUNativeModule;
