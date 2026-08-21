import {
  GPUBufferUsage,
  GPUColorWrite,
  GPUMapMode,
  GPUShaderStage,
  GPUTextureUsage,
} from "./constants";

// Globals that this function installs on the calling runtime. These are the
// native-derived flag constants re-exported from `./constants` (a single source
// of truth, matching the native `wgpu::*Usage` enums), so they are safe to set
// on any runtime.
const constants = {
  GPUBufferUsage,
  GPUTextureUsage,
  GPUShaderStage,
  GPUColorWrite,
  GPUMapMode,
};

/**
 * Install WebGPU on the runtime that calls it.
 *
 * The native module installs the WebGPU flag constants (`GPUBufferUsage`,
 * `GPUTextureUsage`, `GPUShaderStage`, `GPUColorWrite`, `GPUMapMode`) as globals
 * on the main JS runtime, but worklet runtimes (Reanimated UI, dedicated worklet
 * runtimes, Vision Camera frame processors) start without them, so referencing
 * the bare global inside a worklet yields `undefined`.
 *
 * Call `installWebGPU()` once at the top of a worklet to make those globals
 * available there, instead of importing each constant by hand:
 *
 * ```tsx
 * import { installWebGPU } from "react-native-webgpu";
 *
 * const work = (device: GPUDevice) => {
 *   "worklet";
 *   installWebGPU();
 *   device.createBuffer({
 *     usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
 *   });
 * };
 * ```
 *
 * The constants are captured into the worklet by closure (the same way a shader
 * string is), so they work on every runtime. Calling it on a runtime that
 * already has the globals (e.g. the main JS runtime) is a safe no-op.
 *
 * This is the explicit entry point for runtime setup; for now it only installs
 * the flag constants, but it is the place where other per-runtime WebGPU setup
 * (e.g. `navigator.gpu`) can be wired in later.
 */
export const installWebGPU = () => {
  "worklet";
  const g = globalThis as unknown as Record<string, unknown>;
  for (const [key, value] of Object.entries(constants)) {
    if (g[key] === undefined) {
      g[key] = value;
    }
  }
};
