---
id: installation
title: Installation
sidebar_position: 2
---

# Installation

## Requirements

React Native WebGPU requires:

- **React Native 0.81 or newer.** The library does **not** support the legacy architecture; the New Architecture must be enabled.
- **iOS, Android, macOS, or visionOS.** Prebuilt binaries are provided for visionOS and macOS in addition to iOS and Android.

:::note
The package name on npm is `react-native-wgpu` (not `react-native-webgpu`).
:::

## Install the package

```bash
npm install react-native-wgpu
```

On iOS, install the pods afterwards:

```bash
cd ios && pod install
```

The native binaries (Dawn) are downloaded as part of installation, so there is no extra build step for app developers.

## With Expo

Expo provides a React Native WebGPU template that is preconfigured and works with [react-three-fiber](./integrations/react-three-fiber.md). It runs on iOS, Android, and the Web:

```bash
npx create-expo-app@latest -e with-webgpu
```

This is the fastest way to get a working project, and the recommended starting point if you do not already have an app.

## TypeScript types

The WebGPU global types come from [`@webgpu/types`](https://www.npmjs.com/package/@webgpu/types). Add them to your dev dependencies and reference them so that `navigator.gpu`, `GPUDevice`, `GPUTextureUsage`, and friends are typed:

```bash
npm install --save-dev @webgpu/types
```

```jsonc
// tsconfig.json
{
  "compilerOptions": {
    "types": ["@webgpu/types"]
  }
}
```

## Optional peer dependencies

Some features are opt-in and only pull in extra dependencies when you use them:

| Feature | Packages |
| --- | --- |
| [Reanimated / UI-thread rendering](./differences/reanimated.md) | `react-native-reanimated`, `react-native-worklets` |
| [Three.js](./integrations/three-js.md) | `three` (+ a small Metro config change) |
| [react-three-fiber](./integrations/react-three-fiber.md) | `@react-three/fiber` (+ a `patch-package` patch) |
| [TensorFlow.js](./integrations/tensorflow.md) | `@tensorflow/tfjs`, `@tensorflow/tfjs-backend-webgpu` |
| [Vision Camera](./integrations/vision-camera.md) | `react-native-vision-camera`, `react-native-vision-camera-worklets` |
| [TypeGPU](./integrations/typegpu.md) | `typegpu`, `@typegpu/react`, `unplugin-typegpu` |

Each integration page lists the exact configuration it needs.

## Running the example app

The [example app](https://github.com/wcandillon/react-native-webgpu/tree/main/apps/example) demonstrates every integration on this site. To run it from a clone of the repository:

```bash
git submodule update --init
yarn
cd packages/webgpu
yarn install-dawn   # download the prebuilt Dawn binaries
```

Then build and run the app for your platform (`yarn ios`, `yarn android`, `yarn macos`) from `apps/example`.

### iOS Simulator note

To run on the iOS Simulator you need to disable Metal's API validation, which Dawn does not pass cleanly. In Xcode, open **Edit Scheme** and uncheck **Metal Validation**. See Apple's documentation on [validating your app's Metal API usage](https://developer.apple.com/documentation/xcode/validating-your-apps-metal-api-usage/) for details.
