---
id: react-three-fiber
title: react-three-fiber
sidebar_position: 2
---

# react-three-fiber

[react-three-fiber](https://r3f.docs.pmnd.rs/) (R3F) runs on React Native WebGPU on top of the [Three.js setup](./three-js.md). In addition to the Metro change for Three.js, R3F needs one patch and a custom canvas component.

## Patch `@react-three/fiber`

R3F's `react-native` entry points at a "native" build that has not been kept up to date. Resolve it to the regular web build instead. The simplest way is the Metro resolver:

```js
if (moduleName === "@react-three/fiber") {
  // Use the vanilla web build of react-three-fiber, not the stale "native" path.
  return {
    filePath: path.resolve(
      path.resolve(__dirname, "node_modules/@react-three/fiber"),
      "dist/react-three-fiber.esm.js",
    ),
    type: "sourceFile",
  };
}
```

Alternatively, patch `node_modules/@react-three/fiber/package.json` with [`patch-package`](https://www.npmjs.com/package/patch-package):

```diff
diff --git a/node_modules/@react-three/fiber/package.json b/node_modules/@react-three/fiber/package.json
@@
-  "react-native": "native/dist/react-three-fiber-native.cjs.js",
+  "react-native": "dist/react-three-fiber.cjs.js",
```

## A WebGPU canvas for R3F

R3F's own `<Canvas>` assumes a DOM environment, so build a thin one around `react-native-wgpu`'s `<Canvas>` using R3F's imperative `createRoot` API. The key points are: build a `WebGPURenderer` over the native context (see [`makeWebGPURenderer`](./three-js.md#adapting-the-canvas)), and override `gl.render` so that each render is followed by `context.present()`.

```tsx
import * as THREE from "three";
import React, { useEffect, useRef } from "react";
import type { ReconcilerRoot, RootState } from "@react-three/fiber";
import { extend, createRoot, unmountComponentAtNode, events } from "@react-three/fiber";
import { PixelRatio } from "react-native";
import type { ViewProps } from "react-native";
import { Canvas } from "react-native-wgpu";
import type { CanvasRef } from "react-native-wgpu";

import { makeWebGPURenderer, ReactNativeCanvas } from "./makeWebGPURenderer";

interface FiberCanvasProps {
  children: React.ReactNode;
  style?: ViewProps["style"];
  camera?: THREE.PerspectiveCamera;
  scene?: THREE.Scene;
}

export const FiberCanvas = ({ children, style, scene, camera }: FiberCanvasProps) => {
  const root = useRef<ReconcilerRoot<OffscreenCanvas>>(null!);
  // @ts-expect-error register the THREE namespace with the reconciler
  React.useMemo(() => extend(THREE), []);
  const canvasRef = useRef<CanvasRef>(null);

  useEffect(() => {
    const context = canvasRef.current!.getContext("webgpu")!;
    const renderer = makeWebGPURenderer(context);

    // @ts-expect-error structural canvas adapter
    const canvas = new ReactNativeCanvas(context.canvas) as HTMLCanvasElement;
    canvas.width = canvas.clientWidth * PixelRatio.get();
    canvas.height = canvas.clientHeight * PixelRatio.get();
    const size = { top: 0, left: 0, width: canvas.clientWidth, height: canvas.clientHeight };

    if (!root.current) {
      root.current = createRoot(canvas);
    }
    root.current.configure({
      size,
      events,
      scene,
      camera,
      gl: renderer,
      frameloop: "always",
      dpr: 1,
      onCreated: async (state: RootState) => {
        await state.gl.init();
        const renderFrame = state.gl.render.bind(state.gl);
        state.gl.render = (s: THREE.Scene, c: THREE.Camera) => {
          renderFrame(s, c);
          context?.present(); // React Native only
        };
      },
    });
    root.current.render(children);
    return () => unmountComponentAtNode(canvas);
  });

  return <Canvas ref={canvasRef} style={style} />;
};
```

## Using it

From there the usual declarative R3F tree works, including hooks like `useFrame` and `useThree`:

```tsx
function Box() {
  const ref = useRef<THREE.Mesh>(null!);
  useFrame((_state, delta) => (ref.current.rotation.x += delta));
  return (
    <mesh ref={ref}>
      <boxGeometry args={[1, 1, 1]} />
      <meshStandardMaterial color="orange" />
    </mesh>
  );
}

export const Scene = () => (
  <FiberCanvas style={{ flex: 1 }}>
    <ambientLight intensity={Math.PI / 2} />
    <pointLight position={[10, 10, 10]} />
    <Box />
  </FiberCanvas>
);
```

A complete version, including orbit controls, lives in the [example app](https://github.com/wcandillon/react-native-webgpu/blob/main/apps/example/src/ThreeJS/Fiber.tsx).

:::tip Expo
The Expo template (`npx create-expo-app@latest -e with-webgpu`) ships with react-three-fiber already wired up. It is the quickest way to start an R3F project.
:::
