import "../WebPolyfillGPUModule";

export * from "../Canvas";
export * from "../Offscreen";
export * from "../WebGPUViewNativeComponent";
export * from "../hooks";
export * from "../GPUDeviceProvider";
export * from "../formats";

// We don't need to set all global properties on web, webgpu is already available globally
