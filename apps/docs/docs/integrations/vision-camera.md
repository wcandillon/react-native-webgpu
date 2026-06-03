---
id: vision-camera
title: Vision Camera
sidebar_position: 4
---

# Vision Camera

[react-native-vision-camera](https://react-native-vision-camera.com/) delivers camera frames on a worklet runtime. Combined with React Native WebGPU's [`importExternalTexture`](../differences/external-textures.md) and [Reanimated worklet support](../differences/reanimated.md), you can sample each frame as a `texture_external` and run WGSL effects with zero CPU copies.

The data flow is:

> camera frame → native buffer → `GPUExternalTexture` (NV12, hardware YUV/sRGB conversion) → WGSL effect → `context.present()`

Everything from frame arrival onward runs on Vision Camera's worklet runtime.

## Install

```bash
npm install react-native-vision-camera react-native-vision-camera-worklets \
  react-native-reanimated react-native-worklets
```

Add the Reanimated plugin to `babel.config.js` (see [Reanimated](../differences/reanimated.md)), and configure camera permissions for iOS and Android as described in the Vision Camera docs.

## Create the device on the main thread

Initialize the GPU adapter and device on the JS thread. Feature-detect the native-texture path, and on Android additionally require the YCbCr external-texture feature:

```tsx
const REQUIRED_FEATURES: GPUFeatureName[] = [
  "rnwebgpu/native-texture" as GPUFeatureName,
  "dawn-multi-planar-formats" as GPUFeatureName,
];

// Android-only: gates Dawn's "wrap a YCbCr AHardwareBuffer as a
// GPUExternalTexture" path. Missing on some Vulkan drivers / emulators.
const OPAQUE_YCBCR_EXT =
  "opaque-ycbcr-android-for-external-texture" as GPUFeatureName;

const adapter = await navigator.gpu.requestAdapter();
const featuresToRequest =
  Platform.OS === "android"
    ? [...REQUIRED_FEATURES, OPAQUE_YCBCR_EXT]
    : REQUIRED_FEATURES;
const device = await adapter!.requestDevice({
  requiredFeatures: featuresToRequest,
});
```

## Capture `RNWebGPU` for the worklet

The frame processor runs in a worklet and cannot reach JS-thread globals. Capture the `RNWebGPU` singleton (and the `device`) into locals so the worklet closes over them; the WebGPU serializer ships them across the boundary:

```tsx
// RNWebGPU is a registered, boxable WebGPU object. Capturing it lets the
// worklet build a video frame off RNWebGPU, where the native platform
// context already lives.
const rnwgpu = RNWebGPU;
```

## The frame processor

Use Vision Camera's `useFrameOutput` with `pixelFormat: "native"` for zero-copy NV12 surfaces. In the worklet, turn the native buffer into a video frame, import it as an external texture (passing `rotation` / `mirrored` derived from the frame orientation), render, then release everything:

```tsx
const frameOutput = useFrameOutput({
  pixelFormat: "native", // zero-copy; NV12 IOSurfaces on iOS
  onFrame: (frame) => {
    "worklet";
    if (!pipelineState || !device) {
      frame.dispose();
      return;
    }
    const { pipeline, sampler, context, uniformBuffer } = pipelineState;
    const nativeBuffer = frame.getNativeBuffer();
    try {
      const videoFrame = rnwgpu.createVideoFrameFromNativeBuffer(
        nativeBuffer.pointer,
      );
      try {
        // Map Vision Camera orientation onto Dawn's rotation.
        let rotation: 0 | 90 | 180 | 270 = 0;
        if (frame.orientation === "right") rotation = 90;
        else if (frame.orientation === "down") rotation = 180;
        else if (frame.orientation === "left") rotation = 270;

        const externalTex = device.importExternalTexture({
          source: videoFrame,
          label: "camera-frame",
          rotation,
          mirrored: frame.isMirrored,
        });

        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: externalTex },
            { binding: 1, resource: sampler },
            { binding: 2, resource: { buffer: uniformBuffer } },
          ],
        });

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              clearValue: { r: 0, g: 0, b: 0, a: 1 },
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, bindGroup);
        pass.draw(3);
        pass.end();
        device.queue.submit([encoder.finish()]);

        // Release the surface's access window right after the submit.
        externalTex.destroy();
        context.present();
      } finally {
        videoFrame.release();
      }
    } finally {
      nativeBuffer.release();
      frame.dispose();
    }
  },
});

useCamera({
  isActive: pipelineState != null && cameraDevice != null,
  device: cameraDevice,
  outputs: [frameOutput],
});
```

## Orientation and YUV→RGB

- **iOS:** frames are NV12 `IOSurface`s and the YUV→RGB conversion happens in hardware. `textureSampleBaseClampToEdge` returns RGB directly.
- **Android:** frames are opaque YCbCr `AHardwareBuffer`s. The zero-copy import and the rotation/mirror transform still work, but the sampler returns raw `[Y, Cb, Cr]`, so you apply the YUV→RGB matrix in WGSL. The example app's [`shaders.ts`](https://github.com/wcandillon/react-native-webgpu/blob/main/apps/example/src/VisionCamera/shaders.ts) provides a BT.709 `CAMERA_PRELUDE` you can prepend to your shader.

:::warning Physical device required
Camera capture needs a real device. The iOS Simulator has no camera. Some Android emulators and desktop-class Vulkan drivers do not advertise `opaque-ycbcr-android-for-external-texture`, in which case importing a camera frame as an external texture is unsupported on that hardware.
:::

The complete screen, including a separable blur compute pass and a chromatic-aberration effect, is in the [example app](https://github.com/wcandillon/react-native-webgpu/blob/main/apps/example/src/VisionCamera/VisionCamera.tsx).
