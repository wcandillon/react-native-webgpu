import { installWebGPU } from "react-native-webgpu";
import type { WorkletRuntime } from "react-native-worklets";
import {
  createSerializable,
  createWorkletRuntime,
  scheduleOnRN,
  scheduleOnRuntime,
} from "react-native-worklets";

// Test-harness helpers for the worklet serializer (see packages/webgpu
// WorkletSerialization.spec.ts). They live in app code, not in an eval'd test
// body: the "worklet" directive only works when the babel worklets plugin
// compiles it into the app bundle.

let runtime: WorkletRuntime | null = null;
const getRuntime = () => {
  if (runtime === null) {
    runtime = createWorkletRuntime({ name: "WebGPUSerializerRuntime" });
  }
  return runtime;
};

export interface RoundTripResult {
  size: number;
  label: string;
  isGPUBuffer: boolean;
  sameObjectBack: boolean;
  // Set when the worklet threw, with the error message.
  error?: string;
}

export interface BitmapRoundTripResult {
  width: number;
  height: number;
  isImageBitmap: boolean;
  sameObjectBack: boolean;
  // Set when the worklet threw, with the error message.
  error?: string;
}

export interface CaptureResult {
  threw: boolean;
  message: string;
  typeOnWorklet: string;
}

export interface ConstructorsResult {
  // typeof globalThis.GPURenderBundleEncoder after installWebGPU(). No object
  // of that class ever reaches the runtime, so only installWebGPU() can have
  // installed it (the check three.js makes on every render).
  renderBundleEncoder: string;
  // A pass encoder created on the runtime is an instance of the constructor
  // installWebGPU() installed there...
  passIsComputePassEncoder: boolean;
  // ...and not of an unrelated one.
  passIsRenderBundleEncoder: boolean;
  // A second installWebGPU() keeps the constructor objects already installed.
  stableIdentity: boolean;
  // Set when the worklet threw, with the error message.
  error?: string;
}

// A runtime of its own, so nothing but the device crosses into it before
// installWebGPU() runs there.
let constructorsRuntime: WorkletRuntime | null = null;
const getConstructorsRuntime = () => {
  if (constructorsRuntime === null) {
    constructorsRuntime = createWorkletRuntime({
      name: "WebGPUConstructorsRuntime",
    });
  }
  return constructorsRuntime;
};

export const workletSerialization = {
  // An object carrying jsi::NativeState that is not a WebGPU object: the
  // serializable ref worklets wraps around a plain object.
  foreignNativeStateObject: (): object => createSerializable({ foreign: true }),

  // Sends `buffer` to a worklet runtime and back. Both hops go through the
  // serializer; the second one runs determine/pack on a runtime nothing was
  // ever installed on, which is the Vision Camera frame processor case.
  roundTripBuffer: (buffer: GPUBuffer): Promise<RoundTripResult> =>
    new Promise((resolve) => {
      const done = (
        onWorklet: { size: number; label: string; isGPUBuffer: boolean },
        back: GPUBuffer,
      ) => {
        resolve({ ...onWorklet, sameObjectBack: back === buffer });
      };
      const failed = (error: string) => {
        resolve({
          size: -1,
          label: "",
          isGPUBuffer: false,
          sameObjectBack: false,
          error,
        });
      };
      scheduleOnRuntime(
        getRuntime(),
        (buf: GPUBuffer) => {
          "worklet";
          try {
            installWebGPU();
            // Unboxing installed the GPUBuffer constructor on this runtime.
            // Read it through globalThis: the worklets babel plugin would
            // otherwise capture the main runtime's constructor by closure.
            const ctor = globalThis.GPUBuffer;
            const onWorklet = {
              size: buf.size,
              label: buf.label,
              isGPUBuffer: typeof ctor === "function" && buf instanceof ctor,
            };
            scheduleOnRN(done, onWorklet, buf);
          } catch (e) {
            scheduleOnRN(failed, String(e));
          }
        },
        buffer,
      );
    }),

  // Sends `bitmap` to a worklet runtime and back, as roundTripBuffer does
  // for a buffer. Unboxing it there rebuilds it from its class brand, which
  // only installConstructor registers.
  roundTripImageBitmap: (bitmap: ImageBitmap): Promise<BitmapRoundTripResult> =>
    new Promise((resolve) => {
      const done = (
        onWorklet: { width: number; height: number; isImageBitmap: boolean },
        back: ImageBitmap,
      ) => {
        resolve({ ...onWorklet, sameObjectBack: back === bitmap });
      };
      const failed = (error: string) => {
        resolve({
          width: -1,
          height: -1,
          isImageBitmap: false,
          sameObjectBack: false,
          error,
        });
      };
      scheduleOnRuntime(
        getRuntime(),
        (image: ImageBitmap) => {
          "worklet";
          try {
            installWebGPU();
            // Read through globalThis: the worklets babel plugin would
            // otherwise capture the main runtime's constructor by closure.
            const ctor = globalThis.ImageBitmap;
            const onWorklet = {
              width: image.width,
              height: image.height,
              isImageBitmap:
                typeof ctor === "function" && image instanceof ctor,
            };
            scheduleOnRN(done, onWorklet, image);
          } catch (e) {
            scheduleOnRN(failed, String(e));
          }
        },
        bitmap,
      );
    }),

  // Calls installWebGPU() on a worklet runtime nothing else was installed on
  // and reports the class constructors it finds there (see
  // ConstructorsResult).
  installsConstructors: (device: GPUDevice): Promise<ConstructorsResult> =>
    new Promise((resolve) => {
      const done = (result: ConstructorsResult) => resolve(result);
      scheduleOnRuntime(
        getConstructorsRuntime(),
        (dev: GPUDevice) => {
          "worklet";
          try {
            installWebGPU();
            // Read through globalThis: the worklets babel plugin would
            // otherwise capture the main runtime's constructors by closure.
            const first = globalThis.GPUBuffer;
            installWebGPU();
            const encoder = dev.createCommandEncoder();
            const pass = encoder.beginComputePass();
            const result: ConstructorsResult = {
              renderBundleEncoder: typeof globalThis.GPURenderBundleEncoder,
              passIsComputePassEncoder:
                pass instanceof globalThis.GPUComputePassEncoder,
              passIsRenderBundleEncoder:
                pass instanceof globalThis.GPURenderBundleEncoder,
              stableIdentity: first === globalThis.GPUBuffer,
            };
            pass.end();
            scheduleOnRN(done, result);
          } catch (e) {
            scheduleOnRN(done, {
              renderBundleEncoder: "",
              passIsComputePassEncoder: false,
              passIsRenderBundleEncoder: false,
              stableIdentity: false,
              error: String(e),
            });
          }
        },
        device,
      );
    }),

  // Captures `value` in a worklet and reports whether serializing it threw and
  // what arrived on the worklet runtime. Meant for values the serializer must
  // leave alone, such as Object.create(buffer): it inherits from a WebGPU
  // prototype but is not a WebGPU object.
  captureForeign: (value: object): Promise<CaptureResult> =>
    new Promise((resolve) => {
      const done = (typeOnWorklet: string) =>
        resolve({ threw: false, message: "", typeOnWorklet });
      try {
        scheduleOnRuntime(
          getRuntime(),
          (v: unknown) => {
            "worklet";
            scheduleOnRN(done, typeof v);
          },
          value,
        );
      } catch (e) {
        resolve({
          threw: true,
          message: e instanceof Error ? e.message : String(e),
          typeOnWorklet: "",
        });
      }
    }),
};
