import { registerWebGPUForReanimated } from "../external";
import WebGPUModule from "../NativeWebGPUModule";

export * from "../Canvas";
export * from "../Offscreen";
export * from "../WebGPUViewNativeComponent";
export * from "../hooks";

export { default as WebGPUModule } from "../NativeWebGPUModule";

const _installOk = WebGPUModule.install();

registerWebGPUForReanimated();

if (typeof RNWebGPU !== "undefined") {
  if (!navigator) {
    // @ts-expect-error Navigation object is more complex than this, setting it to an empty object to add gpu property
    navigator = {
      gpu: RNWebGPU.gpu,
      userAgent: "react-native",
    };
  } else {
    navigator.gpu = RNWebGPU.gpu;
    if (typeof navigator.userAgent !== "string") {
      try {
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-ignore - Hermes navigator may not include a userAgent, align with the polyfill if needed
        navigator.userAgent = "react-native";
      } catch {
        // navigator.userAgent can be read-only; ignore if assignment fails
      }
    }
  }
  global.createImageBitmap =
    global.createImageBitmap ?? RNWebGPU.createImageBitmap.bind(RNWebGPU);
} else {
  console.warn(
    `[react-native-wgpu] install() returned ${_installOk} but RNWebGPU global is not available`,
  );
}
