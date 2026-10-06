import type {
  CreateVideoPlayerOptions,
  NativeVideoFrame,
  VideoPlayer,
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
 * VisionCamera's `frame.getNativeBuffer().pointer`) into a `NativeVideoFrame`
 * that keeps it alive until released.
 */
export const createVideoFrameFromNativeBuffer = (
  pointer: bigint,
): NativeVideoFrame =>
  native("createVideoFrameFromNativeBuffer").createVideoFrameFromNativeBuffer(
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
): NativeVideoFrame =>
  native("createTestVideoFrame").createTestVideoFrame(width, height);
