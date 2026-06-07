// WebGPU flag constants as importable JS values.
//
// The native module installs `GPUBufferUsage`, `GPUTextureUsage`,
// `GPUShaderStage`, `GPUColorWrite` and `GPUMapMode` as globals, but only on the
// main JS runtime. Worklet runtimes (Reanimated UI, dedicated worklet runtimes,
// Vision Camera frame processors) do not get those globals, so referencing the
// bare global inside a worklet yields `undefined`.
//
// These are fixed WebGPU spec values (matching the native `wgpu::*Usage` enums),
// so we also expose them as plain JS objects. Importing them into a worklet lets
// the Worklets serializer capture them by closure (the same way module-level
// shader strings are captured), making them available on every runtime without
// passing them in by hand:
//
//   import { GPUBufferUsage } from "react-native-webgpu";
//   const work = () => {
//     "worklet";
//     device.createBuffer({ usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
//   };

export const GPUBufferUsage = {
  MAP_READ: 0x0001,
  MAP_WRITE: 0x0002,
  COPY_SRC: 0x0004,
  COPY_DST: 0x0008,
  INDEX: 0x0010,
  VERTEX: 0x0020,
  UNIFORM: 0x0040,
  STORAGE: 0x0080,
  INDIRECT: 0x0100,
  QUERY_RESOLVE: 0x0200,
};

export const GPUTextureUsage = {
  COPY_SRC: 0x01,
  COPY_DST: 0x02,
  TEXTURE_BINDING: 0x04,
  STORAGE_BINDING: 0x08,
  RENDER_ATTACHMENT: 0x10,
};

export const GPUShaderStage = {
  VERTEX: 0x1,
  FRAGMENT: 0x2,
  COMPUTE: 0x4,
};

export const GPUColorWrite = {
  RED: 0x1,
  GREEN: 0x2,
  BLUE: 0x4,
  ALPHA: 0x8,
  ALL: 0xf,
};

export const GPUMapMode = {
  READ: 0x1,
  WRITE: 0x2,
};
