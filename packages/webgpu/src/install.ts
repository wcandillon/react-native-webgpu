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

// The GPU instance destined for `navigator.gpu` on other runtimes, read from
// the native install like the flags above (the package entry runs the install
// before this module is evaluated). Undefined on web, where the RNWebGPU
// polyfill has no gpu and the browser already provides navigator.gpu. Captured
// into the `installWebGPU` worklet closure: when a worklet is serialized, the
// Worklets custom serializer boxes the GPU object, and unboxing on the target
// runtime installs the GPU prototype there.
const gpu: GPU | undefined =
  typeof RNWebGPU === "undefined" ? undefined : RNWebGPU.gpu;

// The native half of the worklet serializer (see registerWebGPUForReanimated),
// read from the main runtime like `gpu` above and captured into the worklet
// closure: worklets hands a HostObject to every runtime by reference, so it is
// callable on the target runtime without installing anything there. Its
// installConstructors() installs the WebGPU class constructors on the runtime
// that calls it. Undefined on web and node, where the native module is not
// installed.
const helpers = globalThis.__webgpuWorkletHelpers;

/**
 * Install WebGPU on the runtime that calls it.
 *
 * The native module sets up WebGPU on the main JS runtime, but worklet
 * runtimes (Reanimated UI, dedicated worklet runtimes, Vision Camera frame
 * processors) start without it: `navigator.gpu`, the flag constants
 * (`GPUBufferUsage`, `GPUTextureUsage`, `GPUShaderStage`, `GPUColorWrite`,
 * `GPUMapMode`) and the class constructors (`GPUBuffer`, `GPUTexture`,
 * `GPURenderBundleEncoder`, ...) are all `undefined` there. Without the
 * constructors, `instanceof` against a WebGPU class throws a ReferenceError.
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
 * - On a worklet runtime of your own (`createWorkletRuntime`, a frame
 *   processor), results are delivered while an operation is awaited there:
 *   `device.lost` and `uncapturederror` arrive with the next awaited
 *   operation on that runtime, not on their own. The main JS runtime and
 *   the Reanimated UI runtime receive them right away.
 *
 * Everything is captured into the worklet by closure: the constants like a
 * shader string would be, the GPU object through the Worklets custom
 * serializer, which installs the native prototypes on the target runtime when
 * it crosses, and the native helper that installs the class constructors,
 * which worklets hands to every runtime by reference. Promises returned by
 * `requestAdapter`/`requestDevice` settle on the calling runtime (each
 * runtime has its own way of being reached, see RuntimeContext). Calling it on a runtime that already has
 * the globals (e.g. the main JS runtime) is a safe no-op.
 */
export const installWebGPU = () => {
  "worklet";
  const g = globalThis as unknown as Record<string, unknown>;
  for (const [key, value] of Object.entries(constants)) {
    if (g[key] === undefined) {
      g[key] = value;
    }
  }
  if (gpu !== undefined) {
    const nav = g.navigator as { gpu?: GPU; userAgent?: string } | undefined;
    if (nav === undefined) {
      g.navigator = { gpu, userAgent: "react-native" };
    } else if (nav.gpu === undefined) {
      nav.gpu = gpu;
    }
  }
  // The class constructors, so `instanceof` works against any WebGPU class on
  // this runtime (three.js checks `pass instanceof GPURenderBundleEncoder` on
  // every render, for instance). Unboxing an object installs only its own
  // class; this installs the rest and skips the ones already there.
  if (helpers !== undefined) {
    helpers.installConstructors();
  }
};
