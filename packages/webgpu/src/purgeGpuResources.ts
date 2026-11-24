/**
 * Hands the GPU memory nothing uses back to the system: Dawn keeps the memory
 * of destroyed textures and buffers for reuse otherwise. Call `destroy()` on
 * what you are done with first; what is alive is not affected.
 *
 * It covers every device created through react-native-webgpu and returns right
 * away: the work happens in the background, over about a second at most. An
 * imported device is purged by its owner (`Skia.purgeGpuResources()` for
 * Skia's). The system being short of memory triggers it as well.
 *
 * Nothing happens where react-native-webgpu is not installed natively (Web).
 */
export const purgeGpuResources = (): void => {
  if (typeof RNWebGPU === "undefined") {
    return;
  }
  RNWebGPU.purgeGpuResources();
};
