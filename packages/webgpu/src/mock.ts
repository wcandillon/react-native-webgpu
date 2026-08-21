/// <reference types="@webgpu/types" />
import React from "react";
import type { ReactNode } from "react";
import { View } from "react-native";
import type { ViewProps } from "react-native";

import type { NativeCanvas, RNCanvasContext } from "./Canvas";

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

export * from "./constants";
export * from "./install";
export * from "./formats";

const NOT_AVAILABLE_UNDER_JEST =
  "is not available under Jest - react-native-webgpu has no native implementation in tests (see react-native-webgpu/jestSetup.js)";

export type { CanvasRef, NativeCanvas, RNCanvasContext } from "./Canvas";

interface CanvasProps extends ViewProps {
  transparent?: boolean;
  ref?: React.Ref<unknown>;
}

export const Canvas = ({
  transparent: _transparent,
  ref,
  ...props
}: CanvasProps) => {
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
