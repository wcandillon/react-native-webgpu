---
id: tensorflow
title: TensorFlow.js
sidebar_position: 3
---

# TensorFlow.js

[TensorFlow.js](https://www.tensorflow.org/js) runs on React Native WebGPU through its WebGPU backend, [`@tensorflow/tfjs-backend-webgpu`](https://www.npmjs.com/package/@tensorflow/tfjs-backend-webgpu). Because the backend talks to the standard `navigator.gpu`, the same WebGPU device that drives your rendering also accelerates tensor operations: training and inference run on the GPU, on device.

## Install

```bash
npm install @tensorflow/tfjs @tensorflow/tfjs-backend-webgpu
```

## Provide a React Native platform

TensorFlow.js abstracts environment specifics (fetch, text encoding, timing) behind a `Platform`. React Native does not ship one, so register a small implementation. `Buffer` is used for encode/decode, so make sure it is available (it is provided by the standard React Native polyfills, or `buffer`):

```ts
import type { Platform } from "@tensorflow/tfjs-core";
import type { RequestDetails } from "@tensorflow/tfjs-core/dist/io/types";

export class PlatformReactNative implements Platform {
  fetch(path: string, init?: RequestInit, _options?: RequestDetails) {
    return fetch(path, init);
  }
  encode(text: string, encoding: BufferEncoding) {
    return new Uint8Array(Buffer.from(text, encoding));
  }
  decode(bytes: Uint8Array, encoding: BufferEncoding) {
    return Buffer.from(bytes).toString(encoding);
  }
  now() {
    return Date.now();
  }
  setTimeoutCustom() {
    throw new Error("react native does not support setTimeoutCustom");
  }
  isTypedArray(
    a: unknown,
  ): a is Uint8Array | Uint8ClampedArray | Int32Array | Float32Array {
    return (
      a instanceof Float32Array ||
      a instanceof Int32Array ||
      a instanceof Uint8Array ||
      a instanceof Uint8ClampedArray
    );
  }
}
```

## Select the WebGPU backend

Register the platform and import the WebGPU backend, then set it as the active backend before you build models:

```tsx
import * as tf from "@tensorflow/tfjs";
import "@tensorflow/tfjs-backend-webgpu";

import { PlatformReactNative } from "./Platform";

tf.setPlatform("react-native", new PlatformReactNative());

// later, inside an async effect:
await tf.setBackend("webgpu");
console.log("Backend:", tf.getBackend()); // "webgpu"
```

## Example: train and run a model

Once the backend is set, the standard TensorFlow.js API works unchanged. The example app trains a tiny sentiment classifier and runs inference on user input:

```tsx
const model = tf.sequential();
model.add(tf.layers.embedding({ inputDim: 5000, outputDim: 32, inputLength: 100 }));
model.add(tf.layers.globalAveragePooling1d());
model.add(tf.layers.dense({ units: 16, activation: "relu" }));
model.add(tf.layers.dense({ units: 1, activation: "sigmoid" }));
model.compile({
  optimizer: tf.train.adam(0.0005),
  loss: "binaryCrossentropy",
  metrics: ["accuracy"],
});

const xTrain = tf.tensor2d(sequences); // tokenized + padded text
const yTrain = tf.tensor1d(labels);
await model.fit(xTrain, yTrain, { epochs: 50 });

// Inference
const prediction = (await model.predict(inputTensor).data()) as Float32Array;
const score = prediction[0]; // > 0.5 → positive

// Always dispose tensors you create.
xTrain.dispose();
yTrain.dispose();
```

:::tip Memory management
Tensors are GPU resources. Call `.dispose()` on tensors you create (or wrap work in `tf.tidy(...)`) to avoid leaking GPU memory across frames.
:::

The full sentiment-analysis screen is in the [example app](https://github.com/wcandillon/react-native-webgpu/blob/main/apps/example/src/Tensorflow/Tensorflow.tsx).
