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

// The native objects destined for other runtimes (`RNWebGPU` and its `gpu`,
// which becomes `navigator.gpu`), wrapped in a holder object. They cannot be
// read at module evaluation: this module can be evaluated before the native
// install has populated the `RNWebGPU` global, so `main/index.tsx` fills the
// holder right after installing. The holder itself is captured into the
// `installWebGPU` worklet closure; when a worklet is serialized (always after
// startup, so after the holder is filled), the Worklets custom serializer boxes
// the two native objects inside it, and unboxing on the target runtime installs
// their prototypes there.
const holder: { gpu?: GPU; rnwebgpu?: typeof RNWebGPU } = {};

/**
 * @internal Called once by `main/index.tsx` after the native install, so
 * `installWebGPU()` can put `navigator.gpu` and `RNWebGPU` on other runtimes.
 */
export const provideGPUForInstall = (rnwebgpu: typeof RNWebGPU) => {
  holder.rnwebgpu = rnwebgpu;
  holder.gpu = rnwebgpu.gpu;
};

/**
 * Install WebGPU on the runtime that calls it.
 *
 * The native module sets up WebGPU on the main JS runtime, but worklet
 * runtimes (Reanimated UI, dedicated worklet runtimes, Vision Camera frame
 * processors) start without it: `navigator.gpu`, the `RNWebGPU` global and
 * the flag constants (`GPUBufferUsage`, `GPUTextureUsage`, `GPUShaderStage`,
 * `GPUColorWrite`, `GPUMapMode`) are all `undefined` there.
 *
 * Call `installWebGPU()` once at the top of a worklet to make them available:
 *
 * ```tsx
 * import { installWebGPU } from "react-native-webgpu";
 *
 * const work = () => {
 *   "worklet";
 *   installWebGPU();
 *   navigator.gpu.requestAdapter().then((adapter) => {
 *     // ...
 *   });
 * };
 * ```
 *
 * Known limitations:
 *
 * - On react-native-worklets < 0.13.0, read `navigator` through
 *   `globalThis.navigator`: older versions of the Worklets Babel plugin do
 *   not treat a bare `navigator` as a known global, so they capture the main
 *   runtime's `navigator` object by closure instead of reading the one this
 *   function installed.
 * - Spontaneous events are main-runtime only: on a device created on a
 *   worklet runtime, `device.lost` never settles (unless the device is
 *   already lost when read) and `uncapturederror` listeners never fire.
 *   Observe those on a device created on the main JS thread.
 *
 * Everything is captured into the worklet by closure: the constants like a
 * shader string would be, and the `RNWebGPU` and GPU objects through the
 * Worklets custom serializer, which installs the native prototypes on the
 * target runtime when they cross. Promises returned by
 * `requestAdapter`/`requestDevice` settle on the calling runtime (each runtime
 * gets its own async pump). Calling it on a runtime that already has the
 * globals (e.g. the main JS runtime) is a safe no-op.
 */
export const installWebGPU = () => {
  "worklet";
  const g = globalThis as unknown as Record<string, unknown>;
  for (const [key, value] of Object.entries(constants)) {
    if (g[key] === undefined) {
      g[key] = value;
    }
  }
  const { gpu, rnwebgpu } = holder;
  if (rnwebgpu !== undefined && g.RNWebGPU === undefined) {
    g.RNWebGPU = rnwebgpu;
  }
  if (gpu !== undefined) {
    const nav = g.navigator as { gpu?: GPU; userAgent?: string } | undefined;
    if (nav === undefined) {
      g.navigator = { gpu, userAgent: "react-native" };
    } else if (nav.gpu === undefined) {
      nav.gpu = gpu;
    }
  }
};
