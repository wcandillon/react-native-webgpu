---
id: shared-texture-memory
title: Shared Texture Memory
sidebar_position: 4
---

# Shared Texture Memory

React Native WebGPU exposes Dawn's `SharedTextureMemory` so you can import a native pixel surface (an `IOSurface`-backed `CVPixelBuffer` on iOS, an `AHardwareBuffer` on Android) as a sampleable `GPUTexture` **without copying pixels through the CPU**. This is the path you want for camera frames, video frames, or anything coming out of a hardware producer.

This is a lower-level alternative to [`importExternalTexture`](./external-textures.md). Reach for it when you want to manage planes and color conversion yourself; reach for `importExternalTexture` when you want hardware YUV→RGB and lifecycle handled for you.

Like `importExternalTexture`, it is **enabled by default**; there is nothing to request at device creation. The only thing to check is that the device supports it before importing. It always does on iOS/macOS; it can be missing on some Android drivers and emulators.

```tsx
import type { NativeVideoFrame } from "react-native-wgpu";

const adapter = await navigator.gpu.requestAdapter();
const device = await adapter!.requestDevice();

// On by default when supported; this is the only check you need.
if (!device.features.has("rnwebgpu/native-texture" as GPUFeatureName)) {
  return; // rare: some Android drivers/emulators can't import native surfaces
}

// `frame` here is a NativeVideoFrame whose .handle is the native surface
// (IOSurfaceRef / AHardwareBuffer*). NativeVideoFrames are produced by helpers
// like RNWebGPU.createVideoPlayer or RNWebGPU.createTestVideoFrame, or by
// any third-party module that hands you a compatible native pointer.
const memory = device.importSharedTextureMemory({
  handle: frame.handle,
  label: "video-frame",
});
const texture = memory.createTexture();

memory.beginAccess(texture, /* initialized */ true);
// ... bind `texture` into a sampler and render normally ...
memory.endAccess(texture);

texture.destroy();
frame.release();
```

## `beginAccess` / `endAccess`

These bracket the GPU's read window on the shared surface. Pass `initialized: true` when the producer has already written meaningful pixels (the typical video/camera case) and `false` when the next pass will fully overwrite the texture.

Always pair `beginAccess` with `endAccess`, and release the source frame (`frame.release()`) once you are done so the producer can recycle its buffer.
