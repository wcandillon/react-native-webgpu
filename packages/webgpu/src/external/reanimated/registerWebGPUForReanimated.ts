// Installed on every WebGPU prototype by the native module
type WebGPUObject = {
  __webgpuBox: () => { unbox: () => object; __boxedWebGPU: true };
};

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
    const { registerCustomSerializable } = require("react-native-worklets");

    registerCustomSerializable({
      name: "WebGPU",
      determine: (value: object): value is WebGPUObject => {
        "worklet";
        return (
          typeof (value as Partial<WebGPUObject>).__webgpuBox === "function"
        );
      },
      pack: (value: WebGPUObject) => {
        "worklet";
        return value.__webgpuBox();
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
