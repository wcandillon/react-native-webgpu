import type {
  CreateNativeBufferOptions,
  CreateVideoPlayerOptions,
  NativeBuffer,
  VideoPlayer,
  VideoWriter,
  VideoWriterOptions,
} from "./types";

const native = (name: string) => {
  if (typeof RNWebGPU === "undefined") {
    throw new Error(
      `react-native-webgpu is not installed natively; ${name} is unavailable`,
    );
  }
  return RNWebGPU;
};

/**
 * Decodes a video into native frames the GPU samples without any copy.
 *
 * `source` is a URL (`http`, `https`, `file`) or a file path. Playback is
 * controlled like an HTMLMediaElement (`play()`, `pause()`, `currentTime`,
 * `loop`, `volume`); frames are pulled with `copyLatestFrame()` on every
 * render tick and sampled through `importExternalTexture()` or
 * `importSharedTextureMemory()`:
 *
 * ```ts
 * const player = createVideoPlayer(url);
 * player.play();
 * const render = () => {
 *   const frame = player.copyLatestFrame();
 *   if (frame) {
 *     // import, draw, then release once the frame is no longer sampled
 *   }
 *   requestAnimationFrame(render);
 * };
 * ```
 *
 * On Apple platforms the frames are BGRA by default, or NV12 with
 * `{ pixelFormat: "nv12" }`. On Android they are always in the decoder's
 * native YUV layout, for `importExternalTexture()`.
 */
export const createVideoPlayer = (
  source: string,
  options?: CreateVideoPlayerOptions,
): VideoPlayer =>
  native("createVideoPlayer").createVideoPlayer(source, options?.pixelFormat);

/**
 * Wraps a native buffer produced by another library (a `CVPixelBufferRef` on
 * Apple platforms, an `AHardwareBuffer*` on Android, for instance
 * VisionCamera's `frame.getNativeBuffer().pointer`) into a `NativeBuffer`
 * that keeps it alive until released.
 */
export const createNativeBufferFromPointer = (pointer: bigint): NativeBuffer =>
  native("createNativeBufferFromPointer").createNativeBufferFromPointer(
    pointer,
  );

/**
 * Allocates a native frame of the given size filled with a test pattern (a
 * red/green gradient with diagonal stripes in the blue channel), for examples
 * and tests that need a frame without a camera or a video.
 */
export const createTestVideoFrame = (
  width: number,
  height: number,
): NativeBuffer =>
  native("createTestVideoFrame").createTestVideoFrame(width, height);

/**
 * Allocates an empty native buffer (an `AHardwareBuffer` on Android, an
 * `IOSurface` on Apple platforms) that the GPU renders into and a
 * `NativeBufferView` displays: import it with
 * `device.importSharedTextureMemory({ source: frame })`, render into
 * `memory.createTexture()` inside an access, then present it.
 */
export const createNativeBuffer = (
  width: number,
  height: number,
  options?: CreateNativeBufferOptions,
): NativeBuffer =>
  native("createNativeBuffer").createNativeBuffer(width, height, options);

/**
 * Encodes frames drawn on the GPU into an H.264 .mp4 file at `path` (an
 * absolute file path, a file:// URL, or a path relative to the app's cache
 * directory; `writer.path` is where it went), with the platform's hardware
 * encoder:
 * the encoding counterpart of `createVideoPlayer()`.
 *
 * ```ts
 * const writer = createVideoWriter(path, { width, height, frameRate, device });
 * for (let i = 0; i < frames; i++) {
 *   const buffer = await writer.dequeueBuffer();
 *   const memory = device.importSharedTextureMemory({ source: buffer });
 *   const texture = memory.createTexture();
 *   memory.beginAccess(texture, false);
 *   draw(texture, i); // and submit
 *   writer.append(buffer, memory.endAccess(texture), i / frameRate);
 *   texture.destroy();
 *   memory.destroy();
 * }
 * await writer.finish();
 * ```
 *
 * Throws, and its promises reject, with an Error whose `code` is a
 * `VideoWriterErrorCode`. iOS, macOS and Android only.
 */
export const createVideoWriter = (
  path: string,
  options: VideoWriterOptions,
): VideoWriter => native("createVideoWriter").createVideoWriter(path, options);
