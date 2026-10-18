/// <reference types="@webgpu/types" />
import React from "react";
import type { ReactNode } from "react";
import { View } from "react-native";

import type { CanvasProps, NativeCanvas, RNCanvasContext } from "./Canvas";

// Jest mock for `react-native-webgpu`.
//
// There is no native WebGPU implementation under Jest, so this never talks to
// `NativeWebGPUModule` or the native `RNWebGPU` global. `Canvas` renders as a
// plain `View` so screens built around it can still be rendered in tests;
// anything that requires an actual GPU (`importDevice`, `adoptTexture`,
// `useDevice`, a real canvas context) fails loudly instead of silently
// returning fake data, mirroring how the real module already reports a
// missing native install (see `importDevice`/`adoptTexture` in
// `./importDevice.ts`).

export * from "./formats";

// The flag constants. The real package re-exports the globals the native
// module installs (`./constants.ts`); nothing installs them under Jest, so the
// mock defines the spec values itself and, like the native install, also puts
// them on globalThis for code that references the bare globals.
export const GPUBufferUsage: GPUBufferUsage = {
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

export const GPUTextureUsage: GPUTextureUsage = {
  COPY_SRC: 0x01,
  COPY_DST: 0x02,
  TEXTURE_BINDING: 0x04,
  STORAGE_BINDING: 0x08,
  RENDER_ATTACHMENT: 0x10,
  TRANSIENT_ATTACHMENT: 0x20,
};

export const GPUShaderStage: GPUShaderStage = {
  VERTEX: 0x1,
  FRAGMENT: 0x2,
  COMPUTE: 0x4,
};

export const GPUColorWrite: GPUColorWrite = {
  RED: 0x1,
  GREEN: 0x2,
  BLUE: 0x4,
  ALPHA: 0x8,
  ALL: 0xf,
};

export const GPUMapMode: GPUMapMode = {
  READ: 0x1,
  WRITE: 0x2,
};

{
  const g = globalThis as unknown as Record<string, unknown>;
  for (const [key, value] of Object.entries({
    GPUBufferUsage,
    GPUTextureUsage,
    GPUShaderStage,
    GPUColorWrite,
    GPUMapMode,
  })) {
    if (g[key] === undefined) {
      g[key] = value;
    }
  }
}

const NOT_AVAILABLE_UNDER_JEST =
  "is not available under Jest - react-native-webgpu has no native implementation in tests (see react-native-webgpu/jestSetup.js)";

export type {
  AndroidBackingView,
  AndroidCanvasProps,
  AndroidHardwareBufferViewProps,
  AndroidSurfaceViewProps,
  AndroidTextureViewProps,
  CanvasProps,
  CanvasRef,
  NativeCanvas,
  RNCanvasContext,
} from "./Canvas";

// Strip the same props the real Canvas keeps off its outer View.
export const Canvas = ({ android: _android, ref, ...props }: CanvasProps) => {
  React.useImperativeHandle(ref, () => ({
    getContextId: () => -1,
    getContext: (_contextName: "webgpu"): RNCanvasContext | null => null,
    getNativeSurface: (): NativeCanvas => {
      throw new Error(`Canvas.getNativeSurface() ${NOT_AVAILABLE_UNDER_JEST}`);
    },
  }));
  return React.createElement(View, props);
};

export const useCanvasRef = () => React.useRef(null);
export const useSurface = () => ({ ref: React.useRef(null), surface: null });
export const warnIfNotHardwareAccelerated = () => undefined;

export const useDevice = () => ({ adapter: null, device: null });

export const GPUDeviceProvider = ({ children }: { children?: ReactNode }) =>
  children ?? null;

export const useMainDevice = (): never => {
  throw new Error(`useMainDevice() ${NOT_AVAILABLE_UNDER_JEST}`);
};

export const importDevice = (_pointer: bigint): GPUDevice => {
  throw new Error(`importDevice() ${NOT_AVAILABLE_UNDER_JEST}`);
};

export const adoptTexture = (_pointer: bigint): GPUTexture => {
  throw new Error(`adoptTexture() ${NOT_AVAILABLE_UNDER_JEST}`);
};

export const installWebGPU = () => undefined;

export const WebGPUModule = { install: () => false };
