/// <reference types="@webgpu/types" />

import type { NativeCanvas, RNCanvasContext } from "./types";

export * from "./main";
export type {
  VideoFrame,
  VideoPlayer,
  VideoPixelFormat,
  CreateVideoPlayerOptions,
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
    loadVideoFrame: (path: string) => import("./types").VideoFrame;
    createTestVideoFrame: (
      width: number,
      height: number,
    ) => import("./types").VideoFrame;
    createVideoPlayer: (
      path: string,
      pixelFormat?: import("./types").VideoPixelFormat,
    ) => import("./types").VideoPlayer;
    writeTestVideoFile: () => string;
  };

  interface GPUDevice {
    importSharedTextureMemory(
      descriptor: import("./types").GPUSharedTextureMemoryDescriptor,
    ): import("./types").GPUSharedTextureMemory;
  }

  // Extend createImageBitmap to accept ArrayBuffer/TypedArray (encoded image bytes)
  function createImageBitmap(
    image: ArrayBuffer | ArrayBufferView,
  ): Promise<ImageBitmap>;
}
