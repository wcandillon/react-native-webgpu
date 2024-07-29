import WebGPUNativeModule from "./NativeWebGPUModule";

export * from "./Canvas";
export * from "./WebGPUViewNativeComponent";
export { default as WebGPUModule } from "./NativeWebGPUModule";

declare global {
  // eslint-disable-next-line no-var
  var __WebGPUContextRegistry: Record<number, GPUCanvasContext>;
}

WebGPUNativeModule.install();
