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
   * `createWorkletRuntime` / `scheduleOnRuntime`, or a Vision Camera frame
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

// Pixel layout of a NativeBuffer. NOT the WebCodecs `VideoPixelFormat`
// enum — these are the two native surface layouts we support, lower-cased to
// avoid being mistaken for the spec values ("NV12", "BGRA", …).
export type NativeBufferPixelFormat = "bgra8" | "nv12";

// A native, GPU-shareable buffer: a video or camera frame, or a buffer made
// with createNativeBuffer() to render into and show in a NativeBufferView. It
// exists without any GPU device; a device imports it to draw into it or to
// sample it (importSharedTextureMemory, importExternalTexture).
//
// NOT the WebCodecs `VideoFrame`: there is no `close()`/`format`/`timestamp`,
// the surface is referenced by a raw native pointer, and disposal is
// `release()`. The same name as VisionCamera's `NativeBuffer` (a pointer and
// release()), which createNativeBufferFromPointer() wraps into this one.
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
//     frame). importExternalTexture() waits for it on its own; with
//     importSharedTextureMemory(), pass it to beginAccess() through
//     importSharedFence({ type: "sync-fd", handle }). The frame owns the descriptor: import it
//     before release(), and never close it.
//   - release() drops the underlying backing object (a CVPixelBuffer on Apple).
//     The frame is also released when the JS wrapper is garbage-collected; call
//     release() eagerly when you know you're done.
export interface NativeBuffer {
  readonly handle: bigint;
  readonly width: number;
  readonly height: number;
  readonly pixelFormat: NativeBufferPixelFormat;
  readonly acquireFenceFd: bigint | null;
  release(): void;
}

// The color space of a YUV frame, the way its producer reports it: the YCbCr
// matrix and range that encode the samples, and the primaries and transfer
// function of the RGB they encode. Each member is optional; what is left out
// comes from the frame, then from the platform's best guess for the buffer.
export interface NativeBufferColorSpace {
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

// GPUExternalTextureDescriptor with a native buffer as the source: what
// GPUDevice.importExternalTexture() takes on React Native, where there is no
// HTMLVideoElement or WebCodecs VideoFrame. The other members (label,
// colorSpace, and the rotation / mirrored extensions) are the same.
//
// The source is either
//   - a NativeBuffer, imported for this external texture alone: the access
//     begins here and ends at destroy(), like a web external texture, or
//   - a texture from GPUSharedTextureMemory.createTexture(), for a buffer
//     imported once: the external texture can be kept for every frame of the
//     buffer, and is sampled only between the memory's beginAccess() and
//     endAccess().
export interface GPUNativeExternalTextureDescriptor extends Omit<
  GPUExternalTextureDescriptor,
  "source"
> {
  source: NativeBuffer | GPUTexture;
  // Overrides the members of the frame's own color space, for a buffer that
  // carries none (one wrapped from another library) or reports it wrong.
  sourceColorSpace?: NativeBufferColorSpace;
  // The part of the frame the external texture shows, the whole frame by
  // default (Dawn's cropOrigin / cropSize).
  cropOrigin?: GPUOrigin2D;
  cropSize?: GPUExtent2D;
}

// A decoded video, modeled on HTMLMediaElement: play(), pause(), paused,
// currentTime (in seconds, assign it to seek), duration, volume and loop work
// as on the web. Frames are pulled instead of displayed: poll
// copyLatestFrame() on every render tick to obtain the most recently decoded
// frame as an IOSurface/AHardwareBuffer-backed NativeBuffer (null between
// frames, so callers can skip the import work).
export interface VideoPlayer {
  copyLatestFrame(): NativeBuffer | null;
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
  source: NativeBuffer;
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
  pixelFormat?: NativeBufferPixelFormat;
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

export type GPUSharedTextureMemoryDescriptor = {
  label?: string;
} & (
  | {
      // The buffer to import. The memory keeps it alive, and the textures
      // created from it carry its color space for importExternalTexture().
      source: NativeBuffer;
      handle?: undefined;
    }
  | {
      // A raw native handle (IOSurfaceRef on Apple, AHardwareBuffer* on
      // Android) as a BigInt, which the caller keeps alive for as long as the
      // memory (and the textures created from it) are in use.
      handle: bigint;
      source?: undefined;
    }
);

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

/**
 * The options of `createNativeBuffer()`. The buffer holds premultiplied
 * pixels.
 */
export interface CreateNativeBufferOptions {
  /**
   * Defaults to the preferred canvas format of the platform:
   * `"rgba8unorm"` on Android, `"bgra8unorm"` on Apple platforms.
   * `"rgba16float"` works on both; it gives more precision, not extended
   * range: a `NativeBufferView` shows values in [0, 1] and clamps the rest.
   */
  format?: "rgba8unorm" | "bgra8unorm" | "rgba16float";
  /** The primaries of the values. Defaults to `"srgb"`. */
  colorSpace?: PredefinedColorSpace;
}

/**
 * The options of `createVideoWriter()`.
 */
export interface VideoWriterOptions {
  /** The size of the video in pixels, 2 to 8192. */
  width: number;
  height: number;
  /**
   * The nominal frame rate, for the encoder's rate control and key frame
   * interval (fractional rates like 29.97 are fine). Each frame shows at the
   * timestamp it is appended with.
   */
  frameRate: number;
  /**
   * The device the frames are drawn with. On Android the writer copies each
   * frame into the encoder's input surface with it; on Skia, pass the shared
   * device, `importDevice(Skia.getNativeDevice())`.
   */
  device: GPUDevice;
  /**
   * The average bit rate the encoder aims for, in bits per second. Defaults
   * to 12 Mbps. Content that is simple to encode (flat colors, little motion)
   * comes out below it.
   */
  bitrate?: number;
  /**
   * A sound file (a path or a file:// URL) muxed from t = 0 and cut where
   * the video ends: AAC is copied as is, anything else the platform decodes
   * (MP3, WAV, …) is encoded to AAC.
   */
  audio?: string;
}

/**
 * Why a video writer failed: the `code` of the errors it throws and its
 * promises reject with.
 */
export type VideoWriterErrorCode =
  | "unsupported"
  | "invalid-argument"
  | "write-failed"
  | "audio-failed"
  | "encoder-failed"
  | "interrupted"
  | "cancelled";

/**
 * Encodes frames drawn by the GPU into an H.264 .mp4 file with the
 * platform's hardware encoder, the frames staying on the GPU. The encoding
 * counterpart of `VideoPlayer`.
 */
export interface VideoWriter {
  /** The absolute path of the file written. */
  readonly path: string;
  /**
   * A buffer to draw the next frame into, once the encoder can take one:
   * import it with `device.importSharedTextureMemory({ source: buffer })`
   * and draw into `memory.createTexture()` inside an access. On Apple
   * platforms it is the encoder's own buffer: nothing is copied. A buffer
   * released without being appended goes back unencoded.
   */
  dequeueBuffer(): Promise<NativeBuffer>;
  /**
   * Encodes `buffer` as the frame shown at `timestamp` seconds, once the GPU
   * work drawing it is done: `done` is what `memory.endAccess(texture)`
   * returned, after the drawing was submitted. Releases the buffer.
   * Timestamps start at 0 or later and increase. On Android it copies the
   * buffer into the encoder on the calling thread, which waits while the
   * encoder holds all its input.
   */
  append(
    buffer: NativeBuffer,
    done: GPUSharedTextureMemoryEndAccessState | null,
    timestamp: number,
  ): void;
  /**
   * Writes the frames appended so far, then the sound, and closes the file.
   * The last frame lasts 1 / frameRate. Rejects with the writer's first
   * failure, the partial file then deleted.
   */
  finish(): Promise<void>;
  /**
   * Stops the encoder and deletes the partial file. Pending calls reject
   * with "cancelled". Resolves once the file is deleted: await it before
   * writing to the same path again.
   */
  cancel(): Promise<void>;
}
