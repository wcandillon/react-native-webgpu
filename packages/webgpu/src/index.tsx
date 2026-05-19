/// <reference types="@webgpu/types" />

import type {
  GPUSharedTextureMemory,
  GPUSharedTextureMemoryDescriptor,
  NativeCanvas,
  RNCanvasContext,
  VideoPlayer,
} from "./types";

export * from "./main";
export type {
  VideoFrame,
  VideoPlayer,
  GPUSharedTextureMemory,
  GPUSharedTextureMemoryDescriptor,
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

  // Extend createImageBitmap to accept ArrayBuffer/TypedArray (encoded image bytes)
  function createImageBitmap(
    image: ArrayBuffer | ArrayBufferView,
  ): Promise<ImageBitmap>;
}
