import { TurboModuleRegistry } from "react-native";
import type { TurboModule } from "react-native";

export interface Spec extends TurboModule {
  install: () => boolean;
  // Render the native view with the given React tag off-screen into a native
  // GPU-shareable surface (AHardwareBuffer on Android), and return an opaque
  // token. The resolved surface (a BigInt handle + completion fence) is then
  // retrieved synchronously with RNWebGPU.consumeCapturedElement(token).
  captureElement: (tag: number) => Promise<number>;
}

// eslint-disable-next-line import/no-default-export
export default TurboModuleRegistry.getEnforcing<Spec>("WebGPUModule");
