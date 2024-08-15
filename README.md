# React Native WebGPU

React Native implementation of WebGPU using [Dawn](https://dawn.googlesource.com/dawn).  
This is currently a technical preview for early adopters.  

## Installation

Please note that the package name is `react-native-wgpu`.

```
npm install react-native-wgpu
```

## Usage

You can look at the [example](/package/example) folder for working examples.

```tsx
import React, { useEffect, useRef } from "react";
import { StyleSheet, View, PixelRatio } from "react-native";
import type { CanvasRef } from "react-native-wgpu";
import { Canvas } from "react-native-wgpu";

import { redFragWGSL, triangleVertWGSL } from "./triangle";

export function HelloTriangle() {
  const ref = useRef<CanvasRef>(null);

  async function demo() {
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
      throw new Error("No adapter");
    }
    const device = await adapter.requestDevice();
    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();

    const context = ref.current!.getContext("webgpu")!;
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
  }

  useEffect(() => {
    demo();
  }, []);

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

However, there are two differences with the Web: frame scheduling and external textures.

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

### External Textures

External textures are not a concept that exists in React Native.  
Consider the following Web example:

```tsx
const response = await fetch('./assets/img/Di-3d.png');
const imageBitmap = await createImageBitmap(await response.blob());

device.queue.copyExternalImageToTexture(
  { source: imageBitmap },
  { texture: cubeTexture },
  [imageBitmap.width, imageBitmap.height]
);
```

In React Native, you would need to load the texture yourself.  
For instance, we use Skia for image decoding [here](/package/example/src/components/useAssets.ts#L6).

```tsx
const imageBitmap = await decodeImage(require("./assets/Di-3d.png"));

device.queue.writeTexture(
  { texture: cubeTexture, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
  imageBitmap.data.buffer,
  {
    offset: 0,
    bytesPerRow: 4 * imageBitmap.width,
    rowsPerImage: imageBitmap.height,
  },
  { width: imageBitmap.width, height: imageBitmap.height },
);
```

## Troubleshooting

### iOS

To run the React Native WebGPU project on the iOS simulator, you need to disable the Metal validation API.  
In "Edit Scheme," uncheck "Metal Validation."

<img width="1052" alt="Uncheck 'Metal Validation'" src="https://github.com/user-attachments/assets/2676e5cc-e351-4a97-bdc8-22cbd7df2ef2">

### Android

On an Android simulator, a CPU emulation layer is used which may result in very slow performance.

## Library Development

Make sure to check out the submodules:

```
git submodule update --init
```

Make sure you have all the tools required for building the Skia libraries (Android Studio, XCode, Ninja, CMake, Android NDK/build tools).

### Building

* `cd package && yarn`
* `yarn build-dawn`

### Upgrading

1. `git submodule update --remote`
2. `yarn clean-dawn`
3. `yarn build-dawn`

### Codegen

* `cd package && yarn codegen`

### Testing

In the `package` folder, to run the test against Chrome for reference:

```
yarn test:ref
```

To run the e2e test, open the example app on the e2e screen.  
By default, it will try to connect to a localhost test server.  
If you want to run the test suite on a physical device, you can modify the address [here](/package/example/src/useClient.ts#L4).

```
yarn test
```
