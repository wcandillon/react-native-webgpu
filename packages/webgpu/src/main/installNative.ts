import WebGPUModule from "../NativeWebGPUModule";

// Installs the native module: the RNWebGPU global, and the WebGPU globals of
// the main runtime (GPUBufferUsage, GPUTextureUsage, GPUShaderStage...).
//
// A module of its own, imported FIRST (as a bare import) by the package entry
// `../index.tsx` and by `./index.tsx`, because `../constants` reads those
// globals while it is evaluated, and every import of a module is evaluated
// before that module's body. Installing in the body of `./index.tsx` came too
// late: the flags exported by the package were all undefined.
const installOk = WebGPUModule.install();

if (typeof RNWebGPU === "undefined") {
  // install() logs the reason natively (e.g. a null jsi::Runtime on iOS).
  // Nothing works from here on (navigator.gpu, the flag constants, Canvas),
  // so fail at import time rather than leave every export undefined.
  throw new Error(
    `[react-native-webgpu] the native module failed to install (install() returned ${installOk}): the RNWebGPU global is not available. Make sure the native dependencies are installed and rebuild the app.`,
  );
}
