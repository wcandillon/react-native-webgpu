---
id: external-textures
title: External Textures
sidebar_position: 3
---

# External Textures

## Images: `createImageBitmap`

The module provides a `createImageBitmap` function you can use as the source for `copyExternalImageToTexture`, just like on the Web. Resolve a bundled asset to a URL, fetch it, and decode it:

```tsx
import { Image } from "react-native";

const url = Image.resolveAssetSource(require("./assets/image.png")).uri;
const response = await fetch(url);
const imageBitmap = await createImageBitmap(await response.blob());

const texture = device.createTexture({
  size: [imageBitmap.width, imageBitmap.height, 1],
  format: "rgba8unorm",
  usage:
    GPUTextureUsage.TEXTURE_BINDING |
    GPUTextureUsage.COPY_DST |
    GPUTextureUsage.RENDER_ATTACHMENT,
});
device.queue.copyExternalImageToTexture(
  { source: imageBitmap },
  { texture },
  [imageBitmap.width, imageBitmap.height],
);
```

## Video and camera: `importExternalTexture`

`GPUDevice.importExternalTexture` is the high-level path for sampling a native surface (a video frame, a camera frame). You hand it a `NativeVideoFrame` and get back a `GPUExternalTexture` that you bind as a `texture_external` and read with `textureSampleBaseClampToEdge`.

On top of the lower-level [Shared Texture Memory](./shared-texture-memory.md), it does two things for you:

- **Color conversion.** Camera and video surfaces are usually biplanar YUV (NV12), not RGB. An external texture carries the YUV→RGB matrix and the color-space transfer functions, so on the supported paths the sampler returns ready-to-use RGB in hardware.
- **Lifecycle.** It owns the `SharedTextureMemory` + `createTexture` + `beginAccess`/`endAccess` sequence internally, so you just import the frame and `destroy()` the result.

Like the Web, this capability is **enabled by default**; there is nothing to request at device creation. Feature-detect before importing on unknown hardware:

```tsx
const adapter = await navigator.gpu.requestAdapter();
const device = await adapter!.requestDevice();

// On by default when supported; this is the only check you need.
if (!device.features.has("rnwebgpu/native-texture" as GPUFeatureName)) {
  return; // rare: some Android drivers/emulators can't import native surfaces
}

const render = () => {
  // A GPUExternalTexture expires once the queue work that used it is submitted,
  // so re-import one every frame.
  const externalTexture = device.importExternalTexture({
    source: frame, // a NativeVideoFrame
    label: "video-frame",
  });

  const bindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: externalTexture },
      { binding: 1, resource: sampler },
    ],
  });

  // ... encode a pass that samples `externalTexture`, then:
  device.queue.submit([encoder.finish()]);

  // Release the surface's access window right after the submit that sampled it.
  externalTexture.destroy();
  context.present();
};
```

### Orientation: `rotation` and `mirrored`

Camera frames arrive in the sensor's native orientation. To handle that, `importExternalTexture` accepts two **non-spec** options:

- `rotation`: `0 | 90 | 180 | 270` (degrees)
- `mirrored`: a boolean horizontal flip

Dawn bakes these into the sampling transform, so the shader sees an upright frame. They map directly onto Vision Camera's `frame.orientation` and `frame.isMirrored`. See the [Vision Camera integration](../integrations/vision-camera.md) for a complete example.

### Calling `destroy()`

A `GPUExternalTexture` keeps an open access window on the underlying native surface until the wrapper is destroyed. On the Web `importExternalTexture` is core and the lifetime is managed for you; here the window is tied to the JavaScript object's lifetime.

Call `externalTexture.destroy()` right **after** the `queue.submit()` that sampled it (never before) to release the surface back to its producer immediately. `destroy()` is idempotent, and the surface is also released on garbage collection, but relying on GC can starve a producer's buffer pool (for example an `AVPlayer`'s recycled `IOSurface`s) and pile up GPU resources, so prefer the explicit call in a render loop.

:::warning Android YUV→RGB
Hardware YUV→RGB conversion is fully automatic on iOS (NV12 `IOSurface`). On Android, camera frames arrive as an _opaque_ YCbCr `AHardwareBuffer`, and Dawn's Vulkan path forces an identity sampler conversion, so the external sample comes back as raw `[Y, Cb, Cr]`. You still get the zero-copy import and the rotation/mirror transform, but you need to apply the YUV→RGB matrix yourself in the shader. The [Vision Camera example](https://github.com/wcandillon/react-native-webgpu/blob/main/apps/example/src/VisionCamera/shaders.ts) ships a ready-made BT.709 decode (`CAMERA_PRELUDE`).
:::
