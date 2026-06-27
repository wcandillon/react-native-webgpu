/// <reference types="@webgpu/types" />

// WebGPU flag constants as importable JS values.
//
// The native module installs `GPUBufferUsage`, `GPUTextureUsage`,
// `GPUShaderStage`, `GPUColorWrite` and `GPUMapMode` as globals, but only on the
// main JS runtime. Worklet runtimes (Reanimated UI, dedicated worklet runtimes,
// Vision Camera frame processors) do not get those globals, so referencing the
// bare global inside a worklet yields `undefined`.
//
// Rather than hardcode the bit values here (which could drift from the native
// `wgpu::*Usage` enums), we re-export the globals the native module already
// installed (see `GPUBufferUsage.h` and friends, which derive their values from
// the Dawn enums with `static_assert`s). This keeps a single source of truth.
// Importing them into a worklet lets the Worklets serializer capture them by
// closure (the same way module-level shader strings are captured), making them
// available on every runtime without passing them in by hand:
//
//   import { GPUBufferUsage } from "react-native-webgpu";
//   const work = () => {
//     "worklet";
//     device.createBuffer({ usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
//   };
//
// These are read at module evaluation time. The package entry (`index.tsx`)
// re-exports `./main` before `./constants`, and `./main` installs the native
// module synchronously, so the globals always exist by the time this runs.

export const { GPUBufferUsage } = globalThis;

export const { GPUTextureUsage } = globalThis;

export const { GPUShaderStage } = globalThis;

export const { GPUColorWrite } = globalThis;

export const { GPUMapMode } = globalThis;
