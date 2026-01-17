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
 * This is called automatically when the module loads if react-native-worklets is installed.
 */
export const registerWebGPUForReanimated = () => {
  if (isRegistered) {
    return;
  }
  isRegistered = true;

  try {
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
  } catch {
    // react-native-worklets not installed, skip registration
  }
};
