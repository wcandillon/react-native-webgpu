type SurfacePointer = bigint;

export interface NativeCanvas {
  surface: SurfacePointer;
  width: number;
  height: number;
  clientWidth: number;
  clientHeight: number;
}

export type RNCanvasContext = GPUCanvasContext & {
  /**
   * Present the current frame.
   *
   * Only needed when rendering from a **dedicated worklet runtime** (e.g.
   * `createWorkletRuntime` / `runOnRuntime`, or a Vision Camera frame
   * processor), which runs on its own thread. On the main JS runtime and the
   * Reanimated UI runtime present is automatic (driven by a global vsync), so
   * calling this there is a no-op. Call it after `queue.submit()`.
   */
  present: () => void;
};

export interface CanvasRef {
  getContextId: () => number;
  getContext(contextName: "webgpu"): RNCanvasContext | null;
  getNativeSurface: () => NativeCanvas;
}

// Pixel layout of a NativeVideoFrame. NOT the WebCodecs `VideoPixelFormat`
// enum — these are the two native surface layouts we support, lower-cased to
// avoid being mistaken for the spec values ("NV12", "BGRA", …).
export type NativeVideoPixelFormat = "bgra8" | "nv12";

// A native, GPU-shareable handle to a single video frame.
//
// NOT the WebCodecs `VideoFrame`: there is no `close()`/`format`/`timestamp`,
// the surface is referenced by a raw native pointer, and disposal is
// `release()`. Named with a `Native` prefix so it doesn't shadow the global
// WebCodecs type or imply spec semantics it doesn't have.
//
//   - handle is the raw pointer (IOSurfaceRef on Apple, AHardwareBuffer* on
//     Android) encoded as a BigInt. Pass it to
//     GPUDevice.importSharedTextureMemory.
//   - pixelFormat describes the surface layout: 'bgra8' for a sampled
//     GPUTexture; 'nv12' (biplanar Y + CbCr) for the importExternalTexture
//     path.
//   - release() drops the underlying backing object (a CVPixelBuffer on Apple).
//     The frame is also released when the JS wrapper is garbage-collected; call
//     release() eagerly when you know you're done.
export interface NativeVideoFrame {
  readonly handle: bigint;
  readonly width: number;
  readonly height: number;
  readonly pixelFormat: NativeVideoPixelFormat;
  release(): void;
}

// A handle to a decoded video stream. Poll copyLatestFrame() each render tick
// to obtain the most recently decoded frame as an IOSurface/AHardwareBuffer
// (returns null between frames so callers can skip the import work).
export interface VideoPlayer {
  copyLatestFrame(): NativeVideoFrame | null;
  play(): void;
  pause(): void;
  release(): void;
}

export interface CreateVideoPlayerOptions {
  // 'bgra8' (default): emit a single-plane BGRA surface, suitable for
  // SharedTextureMemory and a regular sampled GPUTexture.
  // 'nv12': emit biplanar Y + CbCr surfaces, suitable for
  // GPUDevice.importExternalTexture.
  pixelFormat?: NativeVideoPixelFormat;
}

export interface GPUSharedTextureMemoryDescriptor {
  // Raw native handle (IOSurfaceRef on Apple, AHardwareBuffer* on Android),
  // encoded as a BigInt. The caller is responsible for keeping the underlying
  // object alive for as long as the shared memory (and any textures derived
  // from it) are in use. Using NativeVideoFrame.handle handles this
  // automatically.
  handle: bigint;
  label?: string;
}

// The kind of native synchronization primitive a GPUSharedFence wraps, matching
// the shared-fence-* device feature names. Limited to the kinds react-native-webgpu
// targets (iOS/Metal and Android/Vulkan); importSharedFence accepts these and
// export() reports them.
export type GPUSharedFenceType =
  | "mtl-shared-event"
  | "sync-fd"
  | "vk-semaphore-opaque-fd";

export interface GPUSharedFenceDescriptor {
  // The fence kind to import. Must match a shared-fence-* feature enabled on
  // the device.
  type: GPUSharedFenceType;
  // The raw native handle as a BigInt: an id<MTLSharedEvent> pointer for
  // "mtl-shared-event", or an OS file descriptor for the *-fd kinds.
  handle: bigint;
  label?: string;
}

export interface GPUSharedFenceExportInfo {
  type: GPUSharedFenceType;
  // An id<MTLSharedEvent> pointer (Apple) or file descriptor (Android), as a
  // BigInt. The caller takes ownership; e.g. an exported sync-fd must be
  // closed once consumed.
  handle: bigint;
}

// A native GPU synchronization primitive shared across queues/APIs. Produced by
// GPUSharedTextureMemory.endAccess(), consumed by beginAccess(), or imported
// from a consumer's fence with GPUDevice.importSharedFence().
export interface GPUSharedFence {
  readonly __brand: "GPUSharedFence";
  label: string;
  export(): GPUSharedFenceExportInfo;
}

// A fence and the timeline value to wait for (0n for binary sync-fd fences).
export interface GPUSharedFenceState {
  fence: GPUSharedFence;
  signaledValue: bigint;
}

// The result of GPUSharedTextureMemory.endAccess(): each fence is signaled at
// its signaledValue once Dawn's GPU work for the access completes.
export interface GPUSharedTextureMemoryEndAccessState {
  initialized: boolean;
  fences: GPUSharedFenceState[];
}

// Non-standard, Dawn-only device toggles. Mirrors Dawn's DawnTogglesDescriptor
// and is chained onto the native device descriptor at requestDevice time.
// Pass it via the (augmented) GPUDeviceDescriptor: adapter.requestDevice({
// dawnToggles: { enabledToggles: ["dump_shaders"] } }). Toggle names are open
// strings (see Dawn's Toggles.cpp); these flags are Dawn-specific and
// non-portable.
export interface GPUDawnTogglesDescriptor {
  enabledToggles?: string[];
  disabledToggles?: string[];
}

// A piece of shared GPU memory backed by a native surface. Use createTexture()
// to obtain a regular GPUTexture that aliases the surface's pixels. The
// returned texture must be bracketed by beginAccess/endAccess around any
// command-buffer submission that uses it.
export interface GPUSharedTextureMemory {
  readonly __brand: "GPUSharedTextureMemory";
  label: string;
  createTexture(descriptor?: GPUTextureDescriptor): GPUTexture;
  // `initialized` declares whether the surface already holds meaningful pixels
  // (true for an incoming video/camera frame, false if the next pass will fully
  // overwrite it). Optional `fences` are wait fences: Dawn waits for each to
  // reach its signaledValue before writing the surface. Throws if the access
  // could not begin.
  beginAccess(
    texture: GPUTexture,
    initialized: boolean,
    fences?: GPUSharedFenceState[],
  ): void;
  // Ends the access and returns the fences Dawn produced for it. Throws if the
  // access could not be ended.
  endAccess(texture: GPUTexture): GPUSharedTextureMemoryEndAccessState;
}
