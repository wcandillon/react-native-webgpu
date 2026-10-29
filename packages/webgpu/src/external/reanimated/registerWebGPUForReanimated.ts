// The native half of the serializer (WebGPUWorkletHelpers in
// cpp/jsi/NativeObject.h), installed on the main runtime by the native module.
// determine/pack capture it by closure, and worklets hands a HostObject to
// every runtime by reference, so the serializer works wherever a WebGPU object
// is captured (Reanimated UI runtime, createWorkletRuntime, Vision Camera frame
// processors) without installing anything on those runtimes.
interface WebGPUWorkletHelpers {
  // True only for WebGPU objects (their NativeState is a WebGPU class). Other
  // native-state objects, such as Skia's, and objects merely inheriting from a
  // WebGPU prototype are rejected, and no JS property is ever read.
  isNativeObject: (value: unknown) => boolean;
  box: (value: object) => BoxedWebGPUObject;
  // Installs the constructor of every WebGPU class on the calling runtime,
  // skipping the ones already there. installWebGPU() calls it so `instanceof`
  // works on worklet runtimes.
  installConstructors: () => void;
}

interface BoxedWebGPUObject {
  unbox: () => object;
  __boxedWebGPU: true;
}

declare global {
  var __webgpuWorkletHelpers: WebGPUWorkletHelpers | undefined;
  var __webgpuScheduleOnUI: ((job: () => void) => void) | undefined;
}

let isRegistered = false;

/**
 * Register WebGPU objects for Worklets serialization.
 * This allows GPUDevice, GPUCanvasContext, etc. to be passed to worklets.
 *
 * This is called automatically when the module loads if react-native-worklets is installed.
 */
export const registerWebGPUForReanimated = () => {
  if (isRegistered) {
    return;
  }
  isRegistered = true;

  const helpers = globalThis.__webgpuWorkletHelpers;
  if (helpers === undefined) {
    // The native module is not installed (web, node): nothing to serialize.
    return;
  }

  try {
    const {
      registerCustomSerializable,
      scheduleOnUI,
    } = require("react-native-worklets");

    // A promise created on the UI runtime (requestDevice() in a worklet, a
    // mapAsync() there) settles on the UI runtime through a timer the
    // runtime arms on itself; this hook settles it sooner, and delivers
    // device.lost and uncapturederror right away: native hands a function
    // that drains the runtime's mailbox, a host function, to this hook on
    // the main runtime, and worklets runs it on the UI runtime (see
    // cpp/rnwgpu/async/RuntimeContext.h).
    if (typeof scheduleOnUI === "function") {
      globalThis.__webgpuScheduleOnUI = (job: () => void) => {
        scheduleOnUI(() => {
          "worklet";
          job();
        });
      };
    }

    registerCustomSerializable({
      name: "WebGPU",
      determine: (value: object): value is object => {
        "worklet";
        return helpers.isNativeObject(value);
      },
      pack: (value: object) => {
        "worklet";
        return helpers.box(value);
      },
      unpack: (boxed: BoxedWebGPUObject) => {
        "worklet";
        return boxed.unbox();
      },
    });
  } catch {
    // react-native-worklets not installed, skip registration
  }
};
