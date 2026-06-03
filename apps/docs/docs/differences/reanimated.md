---
id: reanimated
title: Reanimated Integration
sidebar_position: 5
---

# Reanimated Integration

React Native WebGPU can run WebGPU rendering on the **UI thread** using [React Native Reanimated](https://docs.swmansion.com/react-native-reanimated/) and [React Native Worklets](https://github.com/margelo/react-native-worklets). This has no equivalent on the Web: it lets your render loop run off the JS thread, which keeps rendering smooth even when JavaScript is busy.

## Install the peer dependencies

These are optional peer dependencies, install them only if you use this feature:

```bash
npm install react-native-reanimated react-native-worklets
```

Make sure the Reanimated Babel plugin is enabled in your `babel.config.js`:

```js
module.exports = {
  presets: ["module:@react-native/babel-preset"],
  plugins: ["react-native-reanimated/plugin"],
};
```

## Passing GPU objects into worklets

When the module loads, WebGPU objects are automatically registered for Worklets serialization. This means you can pass objects like `GPUDevice` and `GPUCanvasContext` **directly** into a worklet, they auto-box on the way in and auto-unbox on the way out:

```tsx
import { Canvas } from "react-native-wgpu";
import { runOnUI } from "react-native-reanimated";

const renderFrame = (device: GPUDevice, context: GPUCanvasContext) => {
  "worklet";
  // WebGPU rendering code runs on the UI thread.
  const commandEncoder = device.createCommandEncoder();
  // ... render ...
  device.queue.submit([commandEncoder.finish()]);
  context.present();
};

// Initialize WebGPU on the main thread, then run on the UI thread.
const device = await adapter.requestDevice();
const context = canvasRef.current.getContext("webgpu");
runOnUI(renderFrame)(device, context);
```

The serializer also ships the `RNWebGPU` singleton and resources such as pipelines, samplers, and buffers across the worklet boundary. This is exactly what makes the [Vision Camera integration](../integrations/vision-camera.md) possible: the camera frame processor runs on Vision Camera's worklet runtime and closes over the `device` and `RNWebGPU` created on the main thread.
