/// <reference types="@webgpu/types" />
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

// The GPU instance destined for `navigator.gpu` on other runtimes, wrapped in
// a holder object. It cannot be read at module evaluation: this module can be
// evaluated before the native install has populated `RNWebGPU.gpu`, so
// `main/index.tsx` fills the holder right after installing. The holder itself
// is captured into the `installWebGPU` worklet closure; when a worklet is
// serialized (always after startup, so after the holder is filled), the
// Worklets custom serializer boxes the GPU object inside it, and unboxing on
// the target runtime installs the GPU prototype there.
const holder: { gpu?: GPU } = {};

/**
 * @internal Called once by `main/index.tsx` after the native install, so
 * `installWebGPU()` can put `navigator.gpu` on other runtimes.
 */
export const provideGPUForInstall = (gpu: GPU) => {
  holder.gpu = gpu;
};

/**
 * Install WebGPU on the runtime that calls it.
 *
 * The native module sets up WebGPU on the main JS runtime, but worklet
 * runtimes (Reanimated UI, dedicated worklet runtimes, Vision Camera frame
 * processors) start without it: `navigator.gpu` and the flag constants
 * (`GPUBufferUsage`, `GPUTextureUsage`, `GPUShaderStage`, `GPUColorWrite`,
 * `GPUMapMode`) are all `undefined` there.
 *
 * Call `installWebGPU()` once at the top of a worklet to make them available:
 *
 * ```tsx
 * import { installWebGPU } from "react-native-webgpu";
 *
 * const work = () => {
 *   "worklet";
 *   installWebGPU();
 *   globalThis.navigator.gpu.requestAdapter().then((adapter) => {
 *     // ...
 *   });
 * };
 * ```
 *
 * Note the `globalThis.` prefix: the Worklets Babel plugin does not treat a
 * bare `navigator` as a known global, so it would capture the main runtime's
 * `navigator` object by closure instead of reading the one this function
 * installed.
 *
 * Everything is captured into the worklet by closure: the constants like a
 * shader string would be, and the GPU object through the Worklets custom
 * serializer, which installs the native prototypes on the target runtime when
 * it crosses. Promises returned by `requestAdapter`/`requestDevice` settle on
 * the calling runtime (each runtime gets its own async pump). Calling it on a
 * runtime that already has the globals (e.g. the main JS runtime) is a safe
 * no-op.
 *
 * Limitation: spontaneous events (`device.lost`, `uncapturederror`) are only
 * delivered to devices created on the main JS runtime.
 */
export const installWebGPU = () => {
  "worklet";
  const g = globalThis as unknown as Record<string, unknown>;
  for (const [key, value] of Object.entries(constants)) {
    if (g[key] === undefined) {
      g[key] = value;
    }
  }
  const { gpu } = holder;
  if (gpu !== undefined) {
    const nav = g.navigator as { gpu?: GPU; userAgent?: string } | undefined;
    if (nav === undefined) {
      g.navigator = { gpu, userAgent: "react-native" };
    } else if (nav.gpu === undefined) {
      nav.gpu = gpu;
    }
  }
};
