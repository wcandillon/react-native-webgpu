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
  CapturedElement,
  GPUCopyElementImageSource,
  GPUCopyElementImageDestination,
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
  CapturedElement,
  ElementSource,
  GPUCopyElementImageSource,
  GPUCopyElementImageDestination,
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
    // Retrieve (and remove) the native surface captured by
    // NativeWebGPUModule.captureElement(), keyed by the token that call
    // resolved with. Returns the AHardwareBuffer*/IOSurface handle and the
    // producer completion fence as BigInts.
    consumeCapturedElement: (token: number) => CapturedElement;
    // Release the native handle returned by consumeCapturedElement, after the
    // SharedTextureMemory import has taken its own reference.
    releaseCapturedElement: (handle: bigint) => void;
  };

  interface GPUDevice {
    importSharedTextureMemory(
      descriptor: GPUSharedTextureMemoryDescriptor,
    ): GPUSharedTextureMemory;
    importSharedFence(descriptor: GPUSharedFenceDescriptor): GPUSharedFence;
  }

  // "HTML in Canvas" (WICG) extension: paint a native child view of a
  // <Canvas layoutSubtree> into a GPUTexture. Async in React Native because the
  // view is rendered on demand (see Element.ts). Resolve the element's native
  // tag with React Native's findNodeHandle(ref) and pass it as `source.source`.
  interface GPUQueue {
    copyElementImageToTexture(
      source: GPUCopyElementImageSource,
      destination: GPUCopyElementImageDestination,
    ): Promise<void>;
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
  }

  // Extend createImageBitmap to accept ArrayBuffer/TypedArray (encoded image bytes)
  function createImageBitmap(
    image: ArrayBuffer | ArrayBufferView,
  ): Promise<ImageBitmap>;
}
