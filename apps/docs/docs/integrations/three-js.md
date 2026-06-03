---
id: three-js
title: Three.js
sidebar_position: 1
---

# Three.js

Starting from **r168**, Three.js runs out of the box with React Native WebGPU through its `WebGPURenderer`. You only need a small Metro configuration change so that `three` resolves to the WebGPU build, plus a thin canvas adapter.

## Metro configuration

Three.js ships several builds. On React Native, Metro would otherwise pick the default bundle; redirect it to `three.webgpu.js` (and `three.tsl.js` for [TSL](https://github.com/mrdoob/three.js/wiki/Three.js-Shading-Language)). The example app's [`metro.config.js`](https://github.com/wcandillon/react-native-webgpu/blob/main/apps/example/metro.config.js) does exactly this:

```js
const { getDefaultConfig, mergeConfig } = require("@react-native/metro-config");
const path = require("path");

const threePackagePath = path.resolve(__dirname, "node_modules/three");
const defaultConfig = getDefaultConfig(__dirname);

const customConfig = {
  resolver: {
    ...defaultConfig.resolver,
    resolveRequest: (context, moduleName, platform) => {
      if (moduleName.startsWith("three/addons/")) {
        return {
          filePath: path.resolve(
            threePackagePath,
            "examples/jsm/" + moduleName.replace("three/addons/", "") + ".js",
          ),
          type: "sourceFile",
        };
      }
      if (moduleName === "three" || moduleName === "three/webgpu") {
        return {
          filePath: path.resolve(threePackagePath, "build/three.webgpu.js"),
          type: "sourceFile",
        };
      }
      if (moduleName === "three/tsl") {
        return {
          filePath: path.resolve(threePackagePath, "build/three.tsl.js"),
          type: "sourceFile",
        };
      }
      return context.resolveRequest(context, moduleName, platform);
    },
    // Allow loading 3D assets through Metro.
    assetExts: [
      ...defaultConfig.resolver.assetExts,
      "glb",
      "gltf",
      "jpg",
      "bin",
      "hdr",
    ],
  },
};

module.exports = mergeConfig(defaultConfig, customConfig);
```

The `assetExts` additions let you `require()` `.glb` / `.gltf` models, HDR environment maps, and textures.

## Adapting the canvas

`WebGPURenderer` expects a DOM-like canvas. Wrap the React Native canvas in a small object that exposes `width` / `height` / `clientWidth` / `clientHeight` and stubs the event methods, then pass both the wrapped canvas and the WebGPU context to the renderer:

```ts
import type { NativeCanvas } from "react-native-wgpu";
import * as THREE from "three";

export class ReactNativeCanvas {
  constructor(private canvas: NativeCanvas) {}

  get width() {
    return this.canvas.width;
  }
  get height() {
    return this.canvas.height;
  }
  set width(width: number) {
    this.canvas.width = width;
  }
  set height(height: number) {
    this.canvas.height = height;
  }
  get clientWidth() {
    return this.canvas.width;
  }
  get clientHeight() {
    return this.canvas.height;
  }
  set clientWidth(width: number) {
    this.canvas.width = width;
  }
  set clientHeight(height: number) {
    this.canvas.height = height;
  }

  addEventListener() {}
  removeEventListener() {}
  dispatchEvent() {}
  setPointerCapture() {}
  releasePointerCapture() {}
}

export const makeWebGPURenderer = (
  context: GPUCanvasContext,
  { antialias = true }: { antialias?: boolean } = {},
) =>
  new THREE.WebGPURenderer({
    antialias,
    // @ts-expect-error the adapter is structurally compatible
    canvas: new ReactNativeCanvas(context.canvas),
    context,
  });
```

## Rendering loop

Initialize the renderer, then present the frame after each render. Remember that frame presentation is [explicit](../differences/frame-scheduling.md):

```tsx
const context = canvasRef.current!.getContext("webgpu")!;
const renderer = makeWebGPURenderer(context);
await renderer.init();

const renderLoop = () => {
  renderer.render(scene, camera);
  context.present(); // React Native only
  requestAnimationFrame(renderLoop);
};
renderLoop();
```

## Model loading polyfill

For loading models, Three.js loaders expect a couple of globals that React Native does not provide. Add the polyfill the example app uses near the top of your entry file:

```ts
import "fast-text-encoding";
window.parent = window;
```

## Next: react-three-fiber

If you prefer the declarative react-three-fiber API, see the [react-three-fiber integration](./react-three-fiber.md), which builds on this setup.
