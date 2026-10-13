import { installWebGPU } from "react-native-webgpu";
import type { WorkletRuntime } from "react-native-worklets";
import {
  createSerializable,
  createWorkletRuntime,
  runOnRuntime,
  scheduleOnRN,
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

export interface CaptureResult {
  threw: boolean;
  message: string;
  typeOnWorklet: string;
}

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
      runOnRuntime(getRuntime(), (buf: GPUBuffer) => {
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
      })(buffer);
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
        runOnRuntime(getRuntime(), (v: unknown) => {
          "worklet";
          scheduleOnRN(done, typeof v);
        })(value);
      } catch (e) {
        resolve({
          threw: true,
          message: e instanceof Error ? e.message : String(e),
          typeOnWorklet: "",
        });
      }
    }),
};
