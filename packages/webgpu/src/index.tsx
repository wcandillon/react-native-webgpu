/// <reference types="@webgpu/types" />
import type {
  GPUSharedFence,
  GPUSharedFenceDescriptor,
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
  GPUSharedFence,
  GPUSharedFenceDescriptor,
  GPUSharedFenceExportInfo,
  GPUSharedFenceState,
  GPUSharedFenceType,
  GPUSharedTextureMemory,
  GPUSharedTextureMemoryDescriptor,
  GPUSharedTextureMemoryEndAccessState,
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
    importSharedFence(descriptor: GPUSharedFenceDescriptor): GPUSharedFence;
  }

  // Extend createImageBitmap to accept ArrayBuffer/TypedArray (encoded image bytes)
  function createImageBitmap(
    image: ArrayBuffer | ArrayBufferView,
  ): Promise<ImageBitmap>;
}
