# React Native WebGPU

React Native implementation of WebGPU using [Dawn](https://dawn.googlesource.com/dawn).  

React Native WebGPU requires React Native 0.81 or newer. It doesn't support the legacy architecture.

## Installation

```
npm install react-native-webgpu
```

> The package was previously published as `react-native-wgpu`. A shim with that name is still available and simply re-exports `react-native-webgpu`.

## With Expo

Expo provides a React Native WebGPU template that works with React Three Fiber.
The works on iOS, Android, and Web.

```
npx create-expo-app@latest -e with-webgpu
```

https://github.com/user-attachments/assets/efbd05f8-4ce0-46c2-919c-03e1095bc8ac

Below are some examples from the [example app](/apps/example/).

https://github.com/user-attachments/assets/116a41b2-2cf8-49f1-9f16-a5c83637c198

Starting from `r168`, Three.js runs out of the box with React Native WebGPU.
You need to have a slight modification of [the metro config](/apps/example/metro.config.js) to resolve Three.js to the WebGPU build.
We also support [react-three-fiber](/apps/example/src/ThreeJS/Fiber.tsx); to make it work, patch `node_modules/@react-three/fiber/package.json` (for instance via `patch-package`) so that it resolves to the WebGPU entry point instead of the React Native bundle:

```diff
diff --git a/node_modules/@react-three/fiber/package.json b/node_modules/@react-three/fiber/package.json
@@
-  "react-native": "native/dist/react-three-fiber-native.cjs.js",
+  "react-native": "dist/react-three-fiber.cjs.js",
```

For model loading, we also need [the following polyfill](/apps/example/src/App.tsx#29).

https://github.com/user-attachments/assets/5b49ef63-0a3c-4679-aeb5-e4b4dddfcc1d

We also provide prebuilt binaries for visionOS and macOS.

https://github.com/user-attachments/assets/2d5c618e-5b15-4cef-8558-d4ddf8c70667

## Usage

Usage is identical to Web.

```tsx
import React from "react";
import { StyleSheet, View, PixelRatio } from "react-native";
import { Canvas, CanvasRef } from "react-native-webgpu";

import { redFragWGSL, triangleVertWGSL } from "./triangle";

export function HelloTriangle() {
  const ref = useRef<CanvasRef>(null);
  useEffect(() => {
    const helloTriangle = async () => {
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) {
        throw new Error("No adapter");
      }
      const device = await adapter.requestDevice();
      const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

      const context = ref.current!.getContext("webgpu")!;
      const canvas = context.canvas as HTMLCanvasElement;
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();

      if (!context) {
        throw new Error("No context");
      }

      context.configure({
        device,
        format: presentationFormat,
        alphaMode: "opaque",
      });

      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: {
          module: device.createShaderModule({
            code: triangleVertWGSL,
          }),
          entryPoint: "main",
        },
        fragment: {
          module: device.createShaderModule({
            code: redFragWGSL,
          }),
          entryPoint: "main",
          targets: [
            {
              format: presentationFormat,
            },
          ],
        },
        primitive: {
          topology: "triangle-list",
        },
      });

      const commandEncoder = device.createCommandEncoder();

      const textureView = context.getCurrentTexture().createView();

      const renderPassDescriptor: GPURenderPassDescriptor = {
        colorAttachments: [
          {
            view: textureView,
            clearValue: [0, 0, 0, 1],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      };

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.setPipeline(pipeline);
      passEncoder.draw(3);
      passEncoder.end();

      device.queue.submit([commandEncoder.finish()]);

      context.present();
    };
    helloTriangle();
  }, [ref]);

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} />
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    flex: 1,
  },
});
```

## Example App

To run the example app you first need to [install Dawn](#installing-dawn).

From there you will be able to run the example app properly.

## Similarities and Differences with the Web

The API has been designed to be completely symmetric with the Web.  
For instance, you can access the WebGPU context synchronously, as well as the canvas size.  
Pixel density and canvas resizing are handled exactly like on the Web as well.

```tsx
// The default canvas size is not scaled to the device pixel ratio
// When resizing the canvas, the clientWidth and clientHeight are updated automatically
// This behaviour is symmetric to the Web
const ctx = canvas.current.getContext("webgpu")!;
ctx.canvas.width = ctx.canvas.clientWidth * PixelRatio.get();
ctx.canvas.height = ctx.canvas.clientHeight * PixelRatio.get();
```

### Frame Scheduling

In React Native, we want to keep frame presentation as a manual operation as we plan to provide more advanced rendering options that are React Native specific.  
This means that when you are ready to present a frame, you need to call `present` on the context.

```tsx
// draw
// submit to the queue
device.queue.submit([commandEncoder.finish()]);
// This method is React Native only
context.present();
```

### Canvas Transparency

On Android, the `alphaMode` property is ignored when configuring the canvas.
To have a transparent canvas by default, use the `transparent` property.

```tsx
return (
  <View style={style.container}>
    <Canvas ref={ref} style={style.webgpu} transparent />
  </View>
);
```

### External Textures

This module provides a `createImageBitmap` function that you can use in `copyExternalImageToTexture`.

```tsx
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

### Shared Texture Memory

React Native WebGPU exposes Dawn's `SharedTextureMemory` so you can import a native pixel surface (an `IOSurface`-backed `CVPixelBuffer` on iOS, an `AHardwareBuffer` on Android) as a sampleable `GPUTexture` without copying pixels through the CPU. This is the path you want for camera frames, video frames, or anything coming out of a hardware producer.

Like `importExternalTexture` on the web, this is **enabled by default**, there is nothing to request at device creation. The only thing to check is that the device supports it before importing. It always does on iOS/macOS; it can be missing on some Android drivers and emulators.

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

`beginAccess`/`endAccess` bracket the GPU's read window on the shared surface. Pass `initialized: true` when the producer has already written meaningful pixels (the typical video/camera case) and `false` when the next pass will fully overwrite the texture.

### Importing External Textures

`GPUDevice.importExternalTexture` is the higher-level path for sampling a native surface. You hand it a `NativeVideoFrame` and get back a `GPUExternalTexture` that you bind as a `texture_external` and read with `textureSampleBaseClampToEdge`. It does two things for you on top of `SharedTextureMemory`:

- **Color conversion.** Camera and video surfaces are usually biplanar YUV (NV12), not RGB. An external texture carries the YUV→RGB matrix and the source/destination color-space transfer functions, so on the supported paths the sampler returns ready-to-use RGB in hardware. With raw `SharedTextureMemory` you would sample the luma/chroma planes and do that conversion by hand in WGSL. This is the main reason to prefer it for camera and video frames.
- **Lifecycle.** It owns the `SharedTextureMemory` + `createTexture` + `beginAccess`/`endAccess` sequence internally, so you just import the frame and `destroy()` the result.

It builds on the same default-on capability as Shared Texture Memory above, so feature-detect the device the same way before importing.

> **Android note:** the hardware YUV→RGB conversion is fully automatic on iOS (NV12 `IOSurface`). On Android, camera frames arrive as an _opaque_ YCbCr `AHardwareBuffer`, and Dawn's Vulkan path forces an identity (`RGB_IDENTITY`) sampler conversion, so the external sample comes back as raw `[Y, Cb, Cr]`. You still get the zero-copy import and the rotation/mirror transform, but you need to apply the YUV→RGB matrix yourself in the shader. See the `CAMERA_PRELUDE` in the [VisionCamera example](/apps/example/src/VisionCamera/shaders.ts) for a ready-made BT.709 decode.

```tsx
const adapter = await navigator.gpu.requestAdapter();
const device = await adapter!.requestDevice();
// Feature-detect as shown above before importing on unsupported hardware.

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

Camera frames arrive in the sensor's native orientation, so `importExternalTexture` also accepts non-spec `rotation` (`0` | `90` | `180` | `270`, in degrees) and `mirrored` (horizontal flip) options. Dawn bakes them into the sampling transform, so the shader sees an upright frame. They map directly onto VisionCamera's `frame.orientation` / `frame.isMirrored`.

#### Calling `destroy()`

A `GPUExternalTexture` keeps an open access window on the underlying native surface until the wrapper is destroyed. On the Web `importExternalTexture` is core and the lifetime is handled for you; here the window is tied to the JavaScript object's lifetime. Call `externalTexture.destroy()` right after the `queue.submit()` that sampled it (never before) to release the surface back to its producer immediately. `destroy()` is idempotent, and the surface is also released when the object is garbage-collected, but relying on GC can starve a producer's buffer pool (e.g. an `AVPlayer`'s recycled `IOSurface`s) and pile up GPU resources, so prefer the explicit call in a render loop.

### Reanimated Integration

React Native WebGPU supports running WebGPU rendering on the UI thread using [React Native Reanimated](https://docs.swmansion.com/react-native-reanimated/) and [React Native Worklets](https://github.com/margelo/react-native-worklets).

First, install the optional peer dependencies:

```sh
npm install react-native-reanimated react-native-worklets
```

WebGPU objects are automatically registered for Worklets serialization when the module loads. You can pass WebGPU objects like `GPUDevice` and `GPUCanvasContext` directly to worklets:

```tsx
import { Canvas } from "react-native-webgpu";
import { runOnUI } from "react-native-reanimated";

const renderFrame = (device: GPUDevice, context: GPUCanvasContext) => {
  "worklet";
  // WebGPU rendering code runs on the UI thread
  const commandEncoder = device.createCommandEncoder();
  // ... render ...
  device.queue.submit([commandEncoder.finish()]);
  context.present();
};

// Initialize WebGPU on main thread, then run on UI thread
const device = await adapter.requestDevice();
const context = canvasRef.current.getContext("webgpu");
runOnUI(renderFrame)(device, context);
```

## Troubleshooting

### iOS

To run the React Native WebGPU project on the iOS simulator, you need to disable the Metal validation API.  
In "Edit Scheme," uncheck "Metal Validation". Learn more [here](https://developer.apple.com/documentation/xcode/validating-your-apps-metal-api-usage/).

<img width="1052" alt="Uncheck 'Metal Validation'" src="https://github.com/user-attachments/assets/2676e5cc-e351-4a97-bdc8-22cbd7df2ef2">

### Android Emulators

By default, Android emulators expose Vulkan through SwiftShader, a software (CPU) renderer. React Native WebGPU runs on it, but rendering is slow and some features are unavailable. When the adapter is software, you will see a "GPUAdapter is not hardware accelerated" warning in the console.

On Apple Silicon, you can instead get a hardware accelerated adapter that runs on the host GPU through MoltenVK (Vulkan on top of Metal):

1. Use a system image at API level 35 or lower. The API 36 image forces software rendering: it reports "system image does not support guest rendering" and falls back to `swiftshader_indirect`.
2. Enable hardware graphics on the AVD. In Android Studio's Device Manager set "Graphics acceleration" to "Hardware", or set `hw.gpu.enabled=yes` and `hw.gpu.mode=host` in the AVD `config.ini`.
3. Launch the emulator with the host GPU and the MoltenVK ICD:

```sh
ANDROID_EMU_VK_ICD=moltenvk emulator -avd <name> -gpu host
```

When this works, the emulator log reports the host GPU instead of SwiftShader (for example `Selecting Vulkan device: Apple M... , MoltenVK is supported, enabling Vulkan portability`) and the warning above no longer appears.

## Library Development

Make sure to check out the submodules:

```
git submodule update --init
```

Make sure you have all the tools required for building the Skia libraries (Android Studio, XCode, Ninja, CMake, Android NDK/build tools).

### Installing Dawn

There is an alternative way which is to install the prebuilt binaries from GitHub.

```sh
$ yarn
$ cd packages/webgpu
$ yarn install-dawn
```

### Building Dawn

Alternatively, you can build Dawn locally.

```sh
yarn
cd packages/webgpu
yarn build-dawn
```

### Upgrading

1. `git submodule update --remote`
2. `yarn clean-dawn`
3. `yarn build-dawn`

### Codegen

* `cd packages/webgpu && yarn codegen`

### Testing

In the `package` folder, to run the test against Chrome for reference:

```
yarn test:ref
```

To run the e2e test, open the example app on the e2e screen.  
By default, it will try to connect to a localhost test server.  
If you want to run the test suite on a physical device, you can modify the address [here](/apps/example/src/useClient.ts#L4).

```
yarn test
```
