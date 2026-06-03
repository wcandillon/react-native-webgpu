---
id: intro
title: Introduction
slug: /intro
sidebar_position: 1
---

# React Native WebGPU

React Native WebGPU is an implementation of the [WebGPU](https://www.w3.org/TR/webgpu/) standard for React Native, built on top of Google's [Dawn](https://dawn.googlesource.com/dawn) engine. It is published on npm as [`react-native-wgpu`](https://www.npmjs.com/package/react-native-wgpu).

It runs on iOS, Android, macOS, and visionOS, and the very same code runs on the Web.

## Why WebGPU

WebGPU is the successor to WebGL. It exposes modern GPU features (compute shaders, storage buffers, explicit pipelines) through a clean, low-level API. Because the API is a web standard, the knowledge and the code you write are portable: a renderer written for the browser runs on a phone, and vice versa.

```tsx
import { Canvas } from "react-native-wgpu";

// `navigator.gpu` is the standard WebGPU entry point, available on native too.
const adapter = await navigator.gpu.requestAdapter();
const device = await adapter!.requestDevice();
```

## Design goal: symmetry with the Web

The API has been designed to be **completely symmetric with the Web**. You access the WebGPU context synchronously, read the canvas size synchronously, and handle pixel density and canvas resizing exactly as you would in the browser.

A handful of behaviors are necessarily React Native specific (frame presentation, canvas transparency on Android, importing native camera and video surfaces). Those are the subject of the [Differences with the Web](./differences/frame-scheduling.md) section, and they are the only things you need to learn on top of standard WebGPU.

## What this site covers

- **[Installation](./installation.md)** — requirements, the package, and the Expo template.
- **[Usage](./usage.md)** — a complete "hello triangle" you can paste in.
- **[Differences with the Web](./differences/frame-scheduling.md)** — frame scheduling, transparency, external textures, shared texture memory, and Reanimated.
- **Integrations** — recipes for [Three.js](./integrations/three-js.md), [react-three-fiber](./integrations/react-three-fiber.md), [TensorFlow.js](./integrations/tensorflow.md), [Vision Camera](./integrations/vision-camera.md), and [TypeGPU](./integrations/typegpu.md).

All of the integrations documented here have a working counterpart in the [example app](https://github.com/wcandillon/react-native-webgpu/tree/main/apps/example).
