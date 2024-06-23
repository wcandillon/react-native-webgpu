import WebGPUNativeModule from "./WebGPUNativeModule";

export { default as WebGPUView } from "./WebGPUViewNativeComponent";
export * from "./WebGPUViewNativeComponent";
export { default as WebGPUModule } from "./WebGPUNativeModule";

declare global {
  // eslint-disable-next-line no-var
  var gpu: GPU;
}

WebGPUNativeModule.install();
export const { gpu } = global;
