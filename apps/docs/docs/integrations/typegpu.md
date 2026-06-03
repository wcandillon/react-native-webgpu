---
id: typegpu
title: TypeGPU
sidebar_position: 5
---

# TypeGPU

[TypeGPU](https://docs.swmansion.com/TypeGPU/) lets you author WGSL in TypeScript with full type safety, and [`@typegpu/react`](https://www.npmjs.com/package/@typegpu/react) adds React hooks for reactive GPU state. Both work on React Native WebGPU on top of the standard `navigator.gpu` device.

## Install

```bash
npm install typegpu @typegpu/react
npm install --save-dev unplugin-typegpu
```

## Babel plugin

TypeGPU compiles tagged shader functions ahead of time. Add `unplugin-typegpu/babel` to your `babel.config.js`. It pairs well with the [Reanimated](../differences/reanimated.md) plugin:

```js
module.exports = {
  presets: ["module:@react-native/babel-preset"],
  plugins: [
    "@babel/plugin-transform-class-static-block",
    "react-native-reanimated/plugin",
    "unplugin-typegpu/babel",
  ],
};
```

## Reactive rendering with `@typegpu/react`

`@typegpu/react` provides hooks that wire a TypeGPU root to a React Native WebGPU canvas: `useRoot`, `useConfigureContext` (returns a canvas `ref` and a context ref), `useMirroredUniform` (mirror React state to a GPU uniform), and `useFrame` (a render loop). Here is the full-screen gradient example from the example app:

```tsx
import { useMemo, useState } from "react";
import { Button, StyleSheet, Text, View } from "react-native";
import { Canvas } from "react-native-wgpu";
import { common, d, std } from "typegpu";
import {
  useConfigureContext,
  useFrame,
  useMirroredUniform,
  useRoot,
} from "@typegpu/react";

export function GradientTiles() {
  const root = useRoot();

  const [spanX, setSpanX] = useState(4);
  const [spanY, setSpanY] = useState(4);

  // Mirror React state to the GPU as a uniform.
  const span = useMirroredUniform(d.vec2u, d.vec2u(spanX, spanY));

  const pipeline = useMemo(() => {
    // A full-screen shader, type-checked in TypeScript.
    return root.createRenderPipeline({
      vertex: common.fullScreenTriangle,
      fragment: ({ uv }) => {
        "use gpu";
        const red = std.floor(uv.x * d.f32(span.$.x)) / d.f32(span.$.x);
        const green = std.floor(uv.y * d.f32(span.$.y)) / d.f32(span.$.y);
        return d.vec4f(red, green, 0.5, 1.0);
      },
    });
  }, [root, span]);

  const { ref, ctxRef } = useConfigureContext();

  useFrame(() => {
    const ctx = ctxRef.current;
    if (!ctx) {
      return;
    }
    pipeline.withColorAttachment({ view: ctx }).draw(3);
    ctx.present?.(); // React Native only
  });

  return (
    <View style={style.container}>
      <Canvas ref={ref} style={style.webgpu} transparent />
      <View style={style.controls}>
        <Text>span x: {spanX}</Text>
        <Button title="➖" onPress={() => setSpanX((p) => Math.max(1, p - 1))} />
        <Button title="➕" onPress={() => setSpanX((p) => Math.min(p + 1, 10))} />
      </View>
    </View>
  );
}

const style = StyleSheet.create({
  container: { flex: 1 },
  webgpu: { aspectRatio: 1 },
  controls: { flex: 1, justifyContent: "center" },
});
```

Note `ctx.present?.()`: as everywhere on React Native, you [present each frame explicitly](../differences/frame-scheduling.md).

## Compute and inference

TypeGPU is equally useful for compute. The example app uses it for a GPU particle simulation (`ComputeBoids`) and for neural-network inference with storage buffers (`MNISTInference`). Both follow the same pattern: a `useRoot()` root, typed buffers and bind groups, and a `useFrame` (or one-shot dispatch) that submits work and presents.

See the [example app](https://github.com/wcandillon/react-native-webgpu/tree/main/apps/example/src/GradientTiles) for the complete TypeGPU screens.
