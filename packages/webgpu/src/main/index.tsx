import { findNodeHandle } from "react-native";

import { registerWebGPUForReanimated } from "../external";
import { provideGPUForInstall } from "../install";
import WebGPUModule from "../NativeWebGPUModule";
import type { GPUDrawElementImageSourceView } from "../types";

// Resolve the `source` of drawElementImageToTexture to a React tag on the
// main JS runtime, where findNodeHandle is available: it handles class
// components and legacy host instances that the native fallback (a number or
// an instance carrying `__nativeTag`) does not.
const resolveElementSource = (source: GPUDrawElementImageSourceView) => {
  if (typeof source === "number") {
    return source;
  }
  const instance =
    source !== null && typeof source === "object" && "current" in source
      ? source.current
      : source;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const tag = instance == null ? null : findNodeHandle(instance as any);
  if (tag === null || tag === undefined) {
    throw new Error(
      "[WebGPU] drawElementImageToTexture: `source` does not resolve to a " +
        "mounted native view (pass a host component ref rendered with " +
        "collapsable={false})",
    );
  }
  return tag;
};

const installDrawElementImageToTexture = () => {
  if (typeof GPUQueue === "undefined") {
    return;
  }
  const native = GPUQueue.prototype.drawElementImageToTexture;
  if (typeof native !== "function") {
    return;
  }
  GPUQueue.prototype.drawElementImageToTexture = function (
    this: GPUQueue,
    source,
    destination,
  ) {
    return native.call(
      this,
      { ...source, source: resolveElementSource(source.source) },
      destination,
    );
  };
};

export * from "../Canvas";
export * from "../Offscreen";
export * from "../WebGPUViewNativeComponent";
export * from "../hooks";
export * from "../GPUDeviceProvider";
export * from "../importDevice";
export * from "../formats";

export { default as WebGPUModule } from "../NativeWebGPUModule";

const _installOk = WebGPUModule.install();

registerWebGPUForReanimated();

if (typeof RNWebGPU !== "undefined") {
  if (!navigator) {
    // @ts-expect-error Navigation object is more complex than this, setting it to an empty object to add gpu property
    navigator = {
      gpu: RNWebGPU.gpu,
      userAgent: "react-native",
    };
  } else {
    navigator.gpu = RNWebGPU.gpu;
    if (typeof navigator.userAgent !== "string") {
      try {
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-ignore - Hermes navigator may not include a userAgent, align with the polyfill if needed
        navigator.userAgent = "react-native";
      } catch {
        // navigator.userAgent can be read-only; ignore if assignment fails
      }
    }
  }
  global.createImageBitmap =
    global.createImageBitmap ?? RNWebGPU.createImageBitmap.bind(RNWebGPU);
  installDrawElementImageToTexture();
  // Let installWebGPU() put navigator.gpu on other runtimes (see install.ts).
  provideGPUForInstall(RNWebGPU.gpu);
} else {
  console.warn(
    `[react-native-webgpu] install() returned ${_installOk} but RNWebGPU global is not available`,
  );
}
