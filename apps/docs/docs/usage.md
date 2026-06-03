---
id: usage
title: Usage
sidebar_position: 3
---

# Usage

Usage is identical to the Web. You render a `<Canvas />`, grab its WebGPU context, and drive it with the standard `navigator.gpu` API. The only React Native specific call in the snippet below is `context.present()`, explained in [Frame Scheduling](./differences/frame-scheduling.md).

## Hello Triangle

```tsx
import React, { useEffect, useRef } from "react";
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
      // The default canvas size is not scaled to the device pixel ratio.
      // This is symmetric with the Web.
      canvas.width = canvas.clientWidth * PixelRatio.get();
      canvas.height = canvas.clientHeight * PixelRatio.get();

      context.configure({
        device,
        format: presentationFormat,
        alphaMode: "opaque",
      });

      const pipeline = device.createRenderPipeline({
        layout: "auto",
        vertex: {
          module: device.createShaderModule({ code: triangleVertWGSL }),
          entryPoint: "main",
        },
        fragment: {
          module: device.createShaderModule({ code: redFragWGSL }),
          entryPoint: "main",
          targets: [{ format: presentationFormat }],
        },
        primitive: { topology: "triangle-list" },
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

      // React Native only: present the frame you just submitted.
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
  container: { flex: 1 },
  webgpu: { flex: 1 },
});
```

## The `Canvas` component

`Canvas` is the only component the library exports. It behaves like an HTML canvas:

- `ref.current.getContext("webgpu")` returns the WebGPU context **synchronously**.
- `context.canvas.width` / `height` are the backing-store size in physical pixels.
- `context.canvas.clientWidth` / `clientHeight` are the layout size in logical points.

The convenience hook `useCanvasRef()` is also available and returns a typed ref:

```tsx
import { Canvas, useCanvasRef } from "react-native-wgpu";

const ref = useCanvasRef();
// ...
<Canvas ref={ref} style={{ flex: 1 }} />;
```

## Next steps

- Learn what is React Native specific in [Differences with the Web](./differences/frame-scheduling.md).
- Drop in a real engine with the [Three.js](./integrations/three-js.md) or [TypeGPU](./integrations/typegpu.md) integration.
