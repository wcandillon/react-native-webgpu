/// <reference types="@webgpu/types" />
import type {
  GPUDawnTogglesDescriptor,
  GPUSharedTextureMemory,
  GPUSharedTextureMemoryDescriptor,
  NativeCanvas,
  RNCanvasContext,
  VideoPlayer,
  VideoFrame,
} from "./types";

export * from "./main";
export type {
  VideoFrame,
  VideoPlayer,
  GPUSharedTextureMemory,
  GPUSharedTextureMemoryDescriptor,
  GPUDawnTogglesDescriptor,
} from "./types";

declare global {
  interface Navigator {
    gpu: GPU;
  }

  var navigator: Navigator;

  var RNWebGPU: {
    gpu: GPU;
    fabric: boolean;
    getNativeSurface: (contextId: number) => NativeCanvas;
    MakeWebGPUCanvasContext: (
      contextId: number,
      width: number,
      height: number,
    ) => RNCanvasContext;
    DecodeToUTF8: (buffer: NodeJS.ArrayBufferView | ArrayBuffer) => string;
    createImageBitmap: typeof createImageBitmap;
    loadVideoFrame: (path: string) => VideoFrame;
    createTestVideoFrame: (width: number, height: number) => VideoFrame;
    createVideoPlayer: (path: string) => VideoPlayer;
    writeTestVideoFile: () => string;
  };

  interface GPUDevice {
    importSharedTextureMemory(
      descriptor: GPUSharedTextureMemoryDescriptor,
    ): GPUSharedTextureMemory;
  }

  // Non-standard, Dawn-only. Lets callers set Dawn device-stage toggles at
  // device creation: adapter.requestDevice({ dawnToggles: { ... } }).
  interface GPUDeviceDescriptor {
    dawnToggles?: GPUDawnTogglesDescriptor;
  }

  // Extend createImageBitmap to accept ArrayBuffer/TypedArray (encoded image bytes)
  function createImageBitmap(
    image: ArrayBuffer | ArrayBufferView,
  ): Promise<ImageBitmap>;
}
