/// <reference types="@webgpu/types" />
import type {
  GPUSharedTextureMemory,
  GPUSharedTextureMemoryDescriptor,
  NativeCanvas,
  RNCanvasContext,
  VideoPlayer,
  NativeVideoFrame,
  NativeVideoPixelFormat,
} from "./types";

export * from "./main";
export type {
  NativeVideoFrame,
  VideoPlayer,
  NativeVideoPixelFormat,
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
    loadVideoFrame: (path: string) => NativeVideoFrame;
    createTestVideoFrame: (width: number, height: number) => NativeVideoFrame;
    // Wrap a NativeBuffer.pointer (CVPixelBufferRef on iOS / AHardwareBuffer*
    // on Android) into a NativeVideoFrame. Matches the shape used by libraries
    // that emit NativeBuffer (e.g. react-native-vision-camera).
    createVideoFrameFromNativeBuffer: (pointer: bigint) => NativeVideoFrame;
    createVideoPlayer: (
      path: string,
      pixelFormat?: NativeVideoPixelFormat,
    ) => VideoPlayer;
    writeTestVideoFile: () => string;
  };

  interface GPUDevice {
    importSharedTextureMemory(
      descriptor: GPUSharedTextureMemoryDescriptor,
    ): GPUSharedTextureMemory;
    // Wrap a NativeBuffer.pointer into a NativeVideoFrame. Reachable from
    // worklet runtimes (e.g. Vision Camera frame processors) because GPUDevice
    // is serialized across worklet boundaries via the WebGPU custom serializer.
    createVideoFrameFromNativeBuffer(pointer: bigint): NativeVideoFrame;
  }

  // Extend createImageBitmap to accept ArrayBuffer/TypedArray (encoded image bytes)
  function createImageBitmap(
    image: ArrayBuffer | ArrayBufferView,
  ): Promise<ImageBitmap>;
}
