import Worklets from "./WorkletsProxy";

// Declare global WebGPU worklet helper functions (installed on main runtime)
declare function __webgpuIsWebGPUObject(obj: unknown): boolean;
declare function __webgpuBox(
  obj: object
): { unbox: () => object; __boxedWebGPU: true };

let isRegistered = false;

/**
 * Register WebGPU objects for Worklets serialization.
 * This allows GPUDevice, GPUCanvasContext, etc. to be passed to worklets.
 * The unbox() method automatically installs the prototype on the UI runtime if needed.
 *
 * Call this once before using WebGPU objects in worklets.
 */
export const registerWebGPUSerializable = () => {
  if (isRegistered) {
    return;
  }
  isRegistered = true;

  Worklets.registerCustomSerializable({
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
