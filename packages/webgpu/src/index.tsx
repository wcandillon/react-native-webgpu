/// <reference types="@webgpu/types" />
import type {
  GPUSharedFence,
  GPUSharedFenceDescriptor,
  GPUDawnTogglesDescriptor,
  GPUSharedTextureMemory,
  GPUSharedTextureMemoryDescriptor,
  NativeCanvas,
  RNCanvasContext,
  VideoPlayer,
  NativeVideoFrame,
  NativeVideoPixelFormat,
} from "./types";

export * from "./main";
export * from "./constants";
export * from "./install";
export type {
  NativeVideoFrame,
  VideoPlayer,
  GPUSharedFence,
  GPUSharedFenceDescriptor,
  GPUSharedFenceExportInfo,
  GPUSharedFenceState,
  GPUSharedFenceType,
  GPUSharedTextureMemory,
  GPUSharedTextureMemoryDescriptor,
  GPUSharedTextureMemoryEndAccessState,
  NativeVideoPixelFormat,
  CreateVideoPlayerOptions,
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
    sessionId: number;
    getNativeSurface: (contextId: number) => NativeCanvas;
    MakeWebGPUCanvasContext: (
      contextId: number,
      width: number,
      height: number,
    ) => RNCanvasContext;
    // Retires a canvas context; called by Canvas on unmount (the Canvas owns
    // the native registry entry for its contextId).
    destroyContext: (contextId: number) => void;
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
    importSharedFence(descriptor: GPUSharedFenceDescriptor): GPUSharedFence;
  }

  // Non-standard, Dawn-only. Lets callers set Dawn device-stage toggles at
  // device creation: adapter.requestDevice({ dawnToggles: { ... } }).
  interface GPUDeviceDescriptor {
    dawnToggles?: GPUDawnTogglesDescriptor;
  }
  // Non-spec extension: camera frames arrive in the sensor's native
  // orientation, which differs between iOS and Android. `rotation` (degrees,
  // one of 0/90/180/270) and `mirrored` (horizontal flip) are baked into the
  // sampling transform by Dawn, so the shader sees an upright frame. Maps
  // directly onto VisionCamera's `frame.orientation` / `frame.isMirrored`.
  interface GPUExternalTextureDescriptor {
    rotation?: 0 | 90 | 180 | 270;
    mirrored?: boolean;
  }

  // Non-spec extension: a GPUExternalTexture imported from a native surface
  // holds an open shared-memory access window on that surface until this
  // wrapper is destroyed. Call destroy() right after the queue.submit() that
  // sampled it (never before) to release the surface back to its producer
  // immediately, instead of waiting for garbage collection. Forgetting to call
  // it is not fatal (GC still cleans up), but it can starve a producer's buffer
  // pool (e.g. a camera/video player) and pile up GPU resources.
  interface GPUExternalTexture {
    destroy(): void;
    // Non-spec extension: a 3x4 row-major matrix (12 numbers) mapping the
    // sampled texel [r, g, b, 1] to gamma-encoded R'G'B'. On the Android
    // opaque-YCbCr camera path, textureSampleBaseClampToEdge returns raw
    // [Y, Cb, Cr] (Dawn hard-codes an RGB_IDENTITY Vulkan conversion); this
    // matrix is derived per-buffer from the driver's suggested YCbCr model and
    // range (BT.601/709/2020, full/narrow). Everywhere else (iOS, RGBA
    // surfaces) it is the identity passthrough, so shaders can apply it
    // unconditionally. Upload it as a uniform and multiply after sampling.
    readonly yuvToRgbMatrix: number[];
  }

  // Extend createImageBitmap to accept ArrayBuffer/TypedArray (encoded image bytes)
  function createImageBitmap(
    image: ArrayBuffer | ArrayBufferView,
  ): Promise<ImageBitmap>;
}
