import { registerWebGPUForReanimated } from "../external";
import WebGPUModule from "../NativeWebGPUModule";

export * from "../Canvas";
export * from "../Offscreen";
export * from "../WebGPUViewNativeComponent";
export * from "../hooks";

export { default as WebGPUModule } from "../NativeWebGPUModule";

const _installOk = WebGPUModule.install();

registerWebGPUForReanimated();

// `RNWebGPU` is only fully populated in the main JS runtime where
// WebGPUModule.install() was called against. When this bundle re-evaluates in
// secondary worklet runtimes (Reanimated UI, Vision Camera frame processor,
// etc.) `RNWebGPU` may either be undefined or be a stripped-down stub. Guard
// every member access so a missing property doesn't take down the whole
// module graph (which would surface as `Cannot read property 'bind' of
// undefined` + every downstream import returning undefined).
if (typeof RNWebGPU !== "undefined" && RNWebGPU != null) {
  if (RNWebGPU.gpu) {
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
  }
  if (
    !global.createImageBitmap &&
    typeof RNWebGPU.createImageBitmap === "function"
  ) {
    global.createImageBitmap = RNWebGPU.createImageBitmap.bind(RNWebGPU);
  }
} else {
  console.warn(
    `[react-native-wgpu] install() returned ${_installOk} but RNWebGPU global is not available`,
  );
}
