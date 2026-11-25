/**
 * Returns the GPU memory nothing uses to the system. Most apps should never
 * call it: `destroy()` lets Dawn reuse the memory, and react-native-webgpu
 * purges on its own when the system is short of memory and, on Android, each
 * time the app goes to the background.
 *
 * It is meant for a one-off peak (a video export) followed by a long stretch
 * that needs far less GPU memory. Not for a render loop, not after each
 * `destroy()`, not on unmount: memory Dawn would have reused has to be
 * requested from the system again. It does not free what was not destroyed.
 *
 * It returns right away and works in the background. It covers every device
 * created through react-native-webgpu; an imported device is purged by its
 * owner. Not part of the WebGPU spec, and does nothing on the Web.
 */
export const purgeGpuResources = (): void => {
  if (typeof RNWebGPU === "undefined") {
    return;
  }
  RNWebGPU.purgeGpuResources();
};
