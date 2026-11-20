// The native frame scheduler (AnimationFramesHostObject in
// cpp/rnwgpu/AnimationFrames.h), installed on the main runtime. Read at module
// scope, like the helpers of `./install`, and captured into the worklets
// below: worklets hands a HostObject to every runtime by reference, and its
// methods act on the runtime that calls them. Undefined on web and node, where
// the browser's requestAnimationFrame is the one to use.
const frames = globalThis.__webgpuAnimationFrames;

/**
 * `requestAnimationFrame`, paced by the display, on any runtime.
 *
 * Drop-in for the global one in a render loop:
 *
 * ```ts
 * import { requestAnimationFrame } from "react-native-webgpu";
 *
 * const render = (timestamp: number) => {
 *   draw(timestamp);
 *   context.present();
 *   requestAnimationFrame(render);
 * };
 * requestAnimationFrame(render);
 * ```
 *
 * React Native's global `requestAnimationFrame` is a timer: on Android it
 * makes the next vsync only when it is requested before that vsync's
 * timestamp, which leaves a loop that requests its next frame at the end of
 * its work about 6 ms of a 120 Hz frame (past that, it drops to 60 fps), and
 * on iOS it runs at 60 Hz. On a worklet runtime of your own
 * (`createWorkletRuntime`), the global one polls every 16 ms (about 48 fps).
 * This one follows the display wherever it is called:
 *
 * - On the React Native runtime, each frame of the display runs the callbacks
 *   requested before it, with the frame's time.
 * - On a worklet runtime of your own, the runtime waits on its thread for the
 *   next frame of the display (the rest of the current frame at most), then
 *   runs them. Nothing else runs on the runtime during that wait.
 * - On the Reanimated UI runtime, it is worklets' `requestAnimationFrame`,
 *   which is paced by the display already.
 *
 * Like a browser's, a callback requested while the frame's callbacks run gets
 * the next frame, and every callback of a frame gets the same timestamp: the
 * time the frame started, on the clock of `performance.now()`. A loop that
 * overruns its frame runs again as soon as it asks, in the frame it overran
 * into, rather than waiting for the frame after.
 *
 * On iPhone, an app only gets more than 60 Hz with
 * `CADisableMinimumFrameDurationOnPhone` set to true in its Info.plist. On
 * macOS and on web, this is the runtime's own `requestAnimationFrame`.
 */
export const requestAnimationFrame = (
  callback: (timestamp: number) => void,
): number => {
  "worklet";
  if (frames === undefined) {
    return globalThis.requestAnimationFrame(callback);
  }
  return frames.request(callback);
};

/**
 * Cancels a callback requested with this package's `requestAnimationFrame`,
 * on the runtime it was requested on. Not interchangeable with the global
 * `cancelAnimationFrame`: each cancels the handles of its own
 * `requestAnimationFrame`.
 */
export const cancelAnimationFrame = (handle: number): void => {
  "worklet";
  if (frames === undefined) {
    globalThis.cancelAnimationFrame(handle);
    return;
  }
  frames.cancel(handle);
};
