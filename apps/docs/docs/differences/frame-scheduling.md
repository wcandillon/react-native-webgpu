---
id: frame-scheduling
title: Frame Scheduling
sidebar_position: 1
---

# Frame Scheduling

The WebGPU API in React Native is designed to be symmetric with the Web: synchronous context access, the same pixel-ratio handling, and the same canvas resizing semantics. There is one deliberate exception, frame presentation.

## `context.present()`

On the Web, the browser presents the canvas for you at the end of a frame. In React Native, frame presentation is kept as an explicit, manual operation. When you are ready to display the frame you have submitted, call `present()` on the context:

```tsx
// draw...
// submit to the queue
device.queue.submit([commandEncoder.finish()]);

// This method is React Native only.
context.present();
```

This is intentional. Keeping presentation explicit leaves room for more advanced, React Native specific rendering options in the future (for example coordinating presentation with the platform's display link). In a render loop you typically call `present()` once per frame, right after `queue.submit`.

## Pixel ratio and resizing

This part is **identical to the Web**. The default canvas size is not scaled to the device pixel ratio, and `clientWidth` / `clientHeight` update automatically when the canvas is laid out. Scale the backing store yourself when you want crisp rendering:

```tsx
import { PixelRatio } from "react-native";

const ctx = canvas.current.getContext("webgpu")!;
ctx.canvas.width = ctx.canvas.clientWidth * PixelRatio.get();
ctx.canvas.height = ctx.canvas.clientHeight * PixelRatio.get();
```

Because the behavior mirrors the browser, code that handles high-DPI rendering on the Web works unchanged here.
