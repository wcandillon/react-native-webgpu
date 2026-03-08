import "../WebPolyfillGPUModule";

export * from "../Canvas";
export * from "../Offscreen";
export * from "../WebGPUViewNativeComponent";
export * from "../hooks";
export type { RNWebGPUInstallOptions, DawnTogglesDescriptor } from "../types";

// We don't need to set all global properties on web, webgpu is already available globally

/** No-op on web — dawnToggles are Dawn-specific and are silently ignored. */
export function installWebGPU(
  _options?: import("../types").RNWebGPUInstallOptions,
): void {
  // no-op on web
}
