import WebGPUModule from "../NativeWebGPUModule";

// Installs the native module: the RNWebGPU global, and the WebGPU globals of
// the main runtime (GPUBufferUsage, GPUTextureUsage, GPUShaderStage...).
//
// A module of its own, imported FIRST by `./index.tsx`, because the other
// imports of that file reach `../constants` (through `../install`), and
// `../constants` reads those globals while it is evaluated. Installing in the
// body of `./index.tsx` came too late: every import of a module is evaluated
// before its body, so the flags exported by the package were all undefined.
const installOk = WebGPUModule.install();

if (typeof RNWebGPU === "undefined") {
  console.warn(
    `[react-native-webgpu] install() returned ${installOk} but RNWebGPU global is not available`,
  );
}
