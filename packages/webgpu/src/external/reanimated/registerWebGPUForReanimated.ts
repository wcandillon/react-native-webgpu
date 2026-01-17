// Declare global WebGPU worklet helper functions (installed by native module)
declare function __webgpuIsWebGPUObject(obj: unknown): boolean;
declare function __webgpuBox(
  obj: object
): { unbox: () => object; __boxedWebGPU: true };

let isRegistered = false;

/**
 * Register WebGPU objects for Worklets serialization.
 * This allows GPUDevice, GPUCanvasContext, etc. to be passed to worklets.
 *
 * Call this once in your App.tsx before using WebGPU objects in worklets:
 *
 * ```tsx
 * import { registerWebGPUForReanimated } from "react-native-wgpu";
 * registerWebGPUForReanimated();
 * ```
 */
export const registerWebGPUForReanimated = () => {
  if (isRegistered) {
    return;
  }
  isRegistered = true;

  // eslint-disable-next-line @typescript-eslint/no-var-requires
  const { registerCustomSerializable } = require("react-native-worklets");

  registerCustomSerializable({
    name: "WebGPU",
    determine: (value: object): value is object => {
      "worklet";
      return __webgpuIsWebGPUObject(value);
    },
    pack: (value: object) => {
      "worklet";
      return __webgpuBox(value);
    },
    unpack: (boxed: { unbox: () => object }) => {
      "worklet";
      return boxed.unbox();
    },
  });
};
