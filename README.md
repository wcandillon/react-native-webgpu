# React Native WebGPU

React Native implementation of WebGPU using [Dawn](https://dawn.googlesource.com/dawn).  

React Native WebGPU requires React Native 0.81 or newer. It doesn't support the legacy architecture.

## Installation

Please note that the package name is `react-native-wgpu`.

```
npm install react-native-wgpu
```

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
import { Canvas, CanvasRef } from "react-native-wgpu";

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

### Reanimated Integration

React Native WebGPU supports running WebGPU rendering on the UI thread using [React Native Reanimated](https://docs.swmansion.com/react-native-reanimated/) and [React Native Worklets](https://github.com/margelo/react-native-worklets).

First, install the optional peer dependencies:

```sh
npm install react-native-reanimated react-native-worklets
```

Then, register WebGPU for Reanimated serialization by calling `registerWebGPUForReanimated()` at the top of your file, before defining any worklets:

```tsx
import { Canvas, registerWebGPUForReanimated } from "react-native-wgpu";
import { runOnUI } from "react-native-reanimated";

// Call this before defining worklets that use WebGPU objects
registerWebGPUForReanimated();

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
