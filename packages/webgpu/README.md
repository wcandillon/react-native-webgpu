# React Native WebGPU

React Native implementation of WebGPU using [Dawn](https://dawn.googlesource.com/dawn).  
This is currently a technical preview for early adopters.  

## Installation

Please note that the package name is `react-native-wgpu`.

```
npm install react-native-wgpu
```

Below are some examples from the [example app](/apps/example/).

https://github.com/user-attachments/assets/116a41b2-2cf8-49f1-9f16-a5c83637c198

Starting from `r168`, Three.js runs out of the box with React Native WebGPU.
You need to have a slight modification of [the metro config](/apps/example/metro.config.js) to resolve Three.js to the WebGPU build.
We also support [three-fiber](/apps/example/src/ThreeJS/Fiber.tsx).
For model loading, we also need [the following polyfill](/apps/example/src/App.tsx#29).

https://github.com/user-attachments/assets/5b49ef63-0a3c-4679-aeb5-e4b4dddfcc1d

We also provide prebuilt binaries for visionOS and macOS.

https://github.com/user-attachments/assets/2d5c618e-5b15-4cef-8558-d4ddf8c70667

## Usage

Currently we recommend to use the `useCanvasEffect` to access the WebGPU context.

```tsx
import React from "react";
import { StyleSheet, View, PixelRatio } from "react-native";
import { Canvas, useCanvasEffect } from "react-native-wgpu";

import { redFragWGSL, triangleVertWGSL } from "./triangle";

export function HelloTriangle() {
  const ref = useCanvasEffect(async () => {
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
  });

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

To run the example app you first need to [build Dawn or download the prebuilt binaries](#building-dawn).

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

React Native WebGPU supports both manual and automatic frame presentation modes:

#### Manual Presentation (Default)
In React Native, frame presentation is kept as a manual operation by default to provide more advanced rendering options that are React Native specific.  
This means that when you are ready to present a frame, you need to call `present` on the context.

```tsx
// draw
// submit to the queue
device.queue.submit([commandEncoder.finish()]);
// This method is React Native only
context.present();
```

#### Automatic Presentation
For a more web-like development experience, you can enable automatic frame presentation by setting the `autoPresent` prop on the Canvas component. When enabled, frames are automatically presented after `device.queue.submit()` calls during the next `requestAnimationFrame` cycle.

```tsx
// Enable auto-present mode
<Canvas ref={ref} style={style.webgpu} autoPresent={true} />

// In your render loop - no need to call context.present()
const render = () => {
  const commandEncoder = device.createCommandEncoder();
  // ... rendering commands ...
  device.queue.submit([commandEncoder.finish()]); // Frame automatically presented
  requestAnimationFrame(render);
};
```

**Note**: Auto-present mode maintains full compatibility with manual presentation - you can still call `context.present()` manually if needed.

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
