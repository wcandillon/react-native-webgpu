/**
 * Wraps an externally created WGPUDevice pointer into a GPUDevice.
 *
 * The canonical use case is adopting the Graphite device from a
 * @shopify/react-native-skia Graphite build:
 *
 * ```ts
 * const device = importDevice(Skia.getNativeDevice());
 * ```
 *
 * This is only sound when the exporting library links the same single Dawn
 * copy as react-native-webgpu and shares its wgpu::Instance (react-native-skia
 * Graphite builds do both).
 */
export const importDevice = (pointer: bigint): GPUDevice => {
  if (typeof RNWebGPU === "undefined") {
    throw new Error(
      "react-native-webgpu is not installed natively; importDevice is unavailable",
    );
  }
  return RNWebGPU.importDevice(pointer);
};

/**
 * Wraps an externally created WGPUTexture pointer into a GPUTexture, taking
 * ownership of one reference: the returned texture releases it when destroyed.
 *
 * Pair with producers that hand out a +1 reference, such as
 * `Skia.Image.MakeNativeTextureFromImage()`:
 *
 * ```ts
 * const texture = adoptTexture(Skia.Image.MakeNativeTextureFromImage(image));
 * ```
 */
export const adoptTexture = (pointer: bigint): GPUTexture => {
  if (typeof RNWebGPU === "undefined") {
    throw new Error(
      "react-native-webgpu is not installed natively; adoptTexture is unavailable",
    );
  }
  return RNWebGPU.adoptTexture(pointer);
};
