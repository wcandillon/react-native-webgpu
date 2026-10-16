type SurfacePointer = bigint;

export interface NativeCanvas {
  surface: SurfacePointer;
  width: number;
  height: number;
  clientWidth: number;
  clientHeight: number;
  // No-op DOM-compatibility stubs so web renderers (Three.js,
  // react-three-fiber) can treat the canvas like an HTMLCanvasElement.
  addEventListener(type: string, listener: EventListener): void;
  removeEventListener(type: string, listener: EventListener): void;
  dispatchEvent(event: Event): void;
  setPointerCapture(pointerId: number): void;
  releasePointerCapture(pointerId: number): void;
}

export type RNCanvasContext = GPUCanvasContext & {
  /**
   * Present the current frame.
   *
   * Call this after `queue.submit()` on every runtime: the main JS runtime, the
   * Reanimated UI runtime, and dedicated worklet runtimes (e.g.
   * `createWorkletRuntime` / `runOnRuntime`, or a Vision Camera frame
   * processor). It runs synchronously on the calling thread, so the frame is
   * presented from whichever thread did the rendering.
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
//   - acquireFenceFd is the sync fd the producer of the frame (the decoder,
//     the camera) signals once it has written the buffer, as a BigInt, on
//     Android; null when the frame has none (Apple platforms, a released
//     frame). importExternalTexture() waits for it on its own; a hardware
//     buffer wrapper takes it through importSharedFence({ type: "sync-fd",
//     handle }) in beginAccess(). The frame owns the descriptor: import it
//     before release(), and never close it.
//   - release() drops the underlying backing object (a CVPixelBuffer on Apple).
//     The frame is also released when the JS wrapper is garbage-collected; call
//     release() eagerly when you know you're done.
export interface NativeVideoFrame {
  readonly handle: bigint;
  readonly width: number;
  readonly height: number;
  readonly pixelFormat: NativeVideoPixelFormat;
  readonly acquireFenceFd: bigint | null;
  release(): void;
}

// The color space of a YUV frame, the way its producer reports it: the YCbCr
// matrix and range that encode the samples, and the primaries and transfer
// function of the RGB they encode. Each member is optional; what is left out
// comes from the frame, then from the platform's best guess for the buffer.
export interface NativeVideoFrameColorSpace {
  matrix?: "identity" | "rec601" | "rec709" | "rec2020";
  range?: "identity" | "narrow" | "full";
  primaries?: "srgb" | "rec709" | "rec601" | "rec2020" | "display-p3";
  transfer?:
    | "identity"
    | "srgb"
    | "display-p3"
    | "smpte-170m"
    | "hlg"
    | "pq"
    | "bt-1886";
}

// A 2D size, as { width, height } or [width, height].
export type GPUExtent2D = { width: number; height: number } | [number, number];

// GPUExternalTextureDescriptor with a NativeVideoFrame as the source: what
// GPUDevice.importExternalTexture() takes on React Native, where there is no
// HTMLVideoElement or WebCodecs VideoFrame. The other members (label,
// colorSpace, and the rotation / mirrored extensions) are the same.
export interface GPUNativeExternalTextureDescriptor extends Omit<
  GPUExternalTextureDescriptor,
  "source"
> {
  source: NativeVideoFrame;
  // Overrides the members of the frame's own color space, for a buffer that
  // carries none (one wrapped from another library) or reports it wrong.
  sourceColorSpace?: NativeVideoFrameColorSpace;
  // The part of the frame the external texture shows, the whole frame by
  // default (Dawn's cropOrigin / cropSize).
  cropOrigin?: GPUOrigin2D;
  cropSize?: GPUExtent2D;
}

// The descriptor of GPUDevice.createNativeBufferExternalTexture(): the same
// options as importExternalTexture(), without the source (the frame is the
// first argument).
export type GPUNativeBufferExternalTextureDescriptor = Omit<
  GPUNativeExternalTextureDescriptor,
  "source"
>;

// The options of GPUDevice.createNativeBufferTexture(). `usage` defaults to
// every usage the buffer allows, as GPUSharedTextureMemory.createTexture()
// without a descriptor.
export interface GPUNativeBufferTextureDescriptor {
  usage?: GPUTextureUsageFlags;
  label?: string;
}

// A native buffer imported once, with the access window of each use explicit:
// the counterpart of GPUHardwareBufferWrapper in Dawn's Android layer
// (androidx.webgpu). A decoder or a camera renders into the same few buffers
// in turn; keep one wrapper per buffer (keyed by NativeVideoFrame.handle), and
// bracket each frame with beginAccess() / endAccess() instead of importing it
// again. The wrapper keeps the frame it was created from alive until
// destroy().
export interface GPUNativeBufferWrapper {
  label: string;
  // Begins an access to the buffer: the GPU waits for each fence to reach its
  // signaledValue first (the producer's fence for the frame, e.g. the frame's
  // acquireFenceFd imported with importSharedFence, 1n for a sync fd). Throws,
  // after a validation error, on a destroyed wrapper or fence, and if Dawn
  // rejects the access (one is already open).
  beginAccess(fences?: GPUSharedFenceState[]): void;
  // Ends the access, after the queue.submit() that used the buffer, and
  // returns the one fence that signals once that GPU work is done (on Android,
  // the sync fds of the access merged into one), or null when there is none.
  // Hand it to the producer before it reuses the buffer, or destroy it.
  endAccess(): GPUSharedFenceState | null;
  // Releases the import (and the frame) now rather than when the object is
  // garbage collected. An access still open is a validation error; it is
  // ended, and its fence is lost. A later use is a validation error and a
  // throw.
  destroy(): void;
}

// A color buffer (an RGBA / R8 / ... AHardwareBuffer, a BGRA IOSurface)
// imported once as a regular texture. Bind `texture` only between
// beginAccess() and endAccess().
export interface GPUNativeBufferTexture extends GPUNativeBufferWrapper {
  readonly __brand: "GPUNativeBufferTexture";
  readonly texture: GPUTexture;
}

// A frame buffer (the YUV AHardwareBuffer of a decoder or a camera, an NV12 or
// BGRA IOSurface) imported once as an external texture, color conversion
// included. Bind `externalTexture` only between beginAccess() and endAccess();
// destroying it on its own (externalTexture.destroy()) only releases the
// external texture, the wrapper keeps the import.
export interface GPUNativeBufferExternalTexture extends GPUNativeBufferWrapper {
  readonly __brand: "GPUNativeBufferExternalTexture";
  readonly externalTexture: GPUExternalTexture;
}

// A decoded video, modeled on HTMLMediaElement: play(), pause(), paused,
// currentTime (in seconds, assign it to seek), duration, volume and loop work
// as on the web. Frames are pulled instead of displayed: poll
// copyLatestFrame() on every render tick to obtain the most recently decoded
// frame as an IOSurface/AHardwareBuffer-backed NativeVideoFrame (null between
// frames, so callers can skip the import work).
export interface VideoPlayer {
  copyLatestFrame(): NativeVideoFrame | null;
  play(): void;
  pause(): void;
  // Stops playback and releases the decoder. Frames already copied stay
  // valid until they are released.
  release(): void;
  readonly paused: boolean;
  // The playback position in seconds. Assign it to seek, also while paused:
  // the next copyLatestFrame() then returns the frame at the new position.
  currentTime: number;
  // The length in seconds, 0 until the metadata has loaded.
  readonly duration: number;
  // 0 to 1.
  volume: number;
  // Restart from the beginning at the end of the stream. Unlike on the web,
  // it defaults to true.
  loop: boolean;
  // The coded size of the frames in pixels, 0 until the metadata has loaded.
  readonly videoWidth: number;
  readonly videoHeight: number;
  // The clockwise rotation to apply when displaying the frames: a video
  // recorded in portrait stores landscape frames with a rotation. Pass it as
  // the `rotation` of importExternalTexture().
  readonly rotation: 0 | 90 | 180 | 270;
  // The nominal frame rate of the video track, 0 if unknown.
  readonly frameRate: number;
}

// The source info of GPUQueue.copyExternalImageToTexture() for a native
// frame (a React Native extension of GPUCopyExternalImageSourceInfo). The
// frame is rendered into the destination through an external texture, which
// converts YUV to RGB and applies `rotation` and `mirrored` (the extension of
// importExternalTexture), so the destination needs
// GPUTextureUsage.RENDER_ATTACHMENT. `origin` and the copy size are in the
// upright frame, after rotation.
export interface GPUCopyExternalNativeFrameSourceInfo {
  source: NativeVideoFrame;
  origin?: GPUOrigin2D;
  flipY?: boolean;
  rotation?: 0 | 90 | 180 | 270;
  mirrored?: boolean;
}

export interface CreateVideoPlayerOptions {
  // The layout of the frames on Apple platforms:
  // 'bgra8' (default): single-plane BGRA surfaces, for importSharedTextureMemory
  // and a regular sampled GPUTexture.
  // 'nv12': biplanar Y + CbCr surfaces, for GPUDevice.importExternalTexture.
  // On Android the decoder always writes its native YUV layout (reported as
  // 'nv12'), which only importExternalTexture samples; the option is ignored.
  pixelFormat?: NativeVideoPixelFormat;
}

// A React Native view to draw with GPUQueue.drawElementImageToTexture: a host
// component ref (`useRef` object), the host instance itself (`ref.current`),
// or the numeric tag returned by findNodeHandle. On worklet runtimes (no
// findNodeHandle) pass the instance or the tag.
export type GPUDrawElementImageSourceView =
  number | { current: unknown } | React.Component | object;

// The source argument of drawElementImageToTexture, modeled on the
// html-in-canvas proposal's GPUDrawElementImageSource. The optional rectangle
// selects the part of the view to draw, in the view's own coordinate space
// and in density-independent points (React Native layout units); it defaults
// to the whole view.
export interface GPUDrawElementImageSource {
  source: GPUDrawElementImageSourceView;
  sourceX?: number;
  sourceY?: number;
  sourceWidth?: number;
  sourceHeight?: number;
  // iOS only (default true): flush pending layout and layer changes before
  // drawing, so a snapshot taken right after a state change reflects it. Pass
  // false to draw the last committed frame instead, which skips a round trip
  // to the render server; the choice for repeated captures. Android renders
  // the views' current display lists either way.
  afterScreenUpdates?: boolean;
}

// The destination argument of drawElementImageToTexture:
// GPUCopyExternalImageDestInfo plus the pixel size of the region the view is
// drawn into. `size` defaults to the source rectangle's natural pixel size
// (points times the device pixel ratio); pass it explicitly to scale the view
// to the destination region.
export interface GPUDrawElementImageDestination extends GPUCopyExternalImageDestInfo {
  size?: GPUExtent3DStrict;
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
  "mtl-shared-event" | "sync-fd" | "vk-semaphore-opaque-fd";

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
  // Throws, after a validation error says why, if the fence was destroyed or
  // Dawn can't export it.
  export(): GPUSharedFenceExportInfo;
  // Releases the fence now (closing its sync fd on Android) rather than when
  // the object is garbage collected. A later use is a validation error, and
  // export() and beginAccess() then throw.
  destroy(): void;
}

// A fence and the timeline value to wait for. Binary fences (sync-fd on
// Vulkan) must use 1n: Dawn rejects any other value.
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
  // Releases the memory (the imported native buffer) now rather than when the
  // object is garbage collected. End the access first (a destroy() during one
  // is a validation error, and the access stays open) and destroy the textures
  // created from it. A later use is a validation error: createTexture() then
  // returns an invalid texture, beginAccess() and endAccess() throw.
  destroy(): void;
}
