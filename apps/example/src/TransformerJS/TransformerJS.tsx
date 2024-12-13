import { View } from "react-native";

// Import TensorFlow.js and required modules
import * as tf from "@tensorflow/tfjs";
import "@tensorflow/tfjs-backend-webgpu";
import { Platform } from "@tensorflow/tfjs-core";
import { GPUOffscreenCanvas } from "./OffscreenCanvas";

global.OffscreenCanvas = GPUOffscreenCanvas;

class PlatformReactNative implements Platform {
  /**
   * Makes an HTTP request.
   *
   * see @fetch docs above.
   */
  async fetch(
    path: string,
    init?: RequestInit,
    options?: tf.io.RequestDetails,
  ) {
    return fetch(path, init, options);
  }

  /**
   * Encode the provided string into an array of bytes using the provided
   * encoding.
   */
  encode(text: string, encoding: string): Uint8Array {
    // See https://www.w3.org/TR/encoding/#utf-16le
    if (encoding === "utf-16") {
      encoding = "utf16le";
    }
    return new Uint8Array(Buffer.from(text, encoding as BufferEncoding));
  }
  /** Decode the provided bytes into a string using the provided encoding. */
  decode(bytes: Uint8Array, encoding: string): string {
    // See https://www.w3.org/TR/encoding/#utf-16le
    if (encoding === "utf-16") {
      encoding = "utf16le";
    }
    return Buffer.from(bytes).toString(encoding as BufferEncoding);
  }

  now(): number {
    //@ts-ignore
    if (global.nativePerformanceNow) {
      //@ts-ignore
      return global.nativePerformanceNow();
    }
    return Date.now();
  }

  setTimeoutCustom() {
    throw new Error("react native does not support setTimeoutCustom");
  }

  isTypedArray(
    a: unknown,
  ): a is Uint8Array | Float32Array | Int32Array | Uint8ClampedArray {
    return (
      a instanceof Float32Array ||
      a instanceof Int32Array ||
      a instanceof Uint8Array ||
      a instanceof Uint8ClampedArray
    );
  }
}

tf.setPlatform("react-native", new PlatformReactNative());

(async () => {
  console.log("Starting TensorFlow.js with WebGPU backend...");
  // Set the backend to WebGPU
  const result = await tf.setBackend("webgpu");
  console.log("WebGPU backend is ready: " + result);
  console.log("tf.getBackend()", tf.getBackend());
  const xs = tf.linspace(-1, 1, 100);
  const ys = xs
    .mul(2)
    .sub(1)
    .add(tf.randomNormal([100], 0, 0.1));

  // Create a simple model with one dense layer.
  const model = tf.sequential();
  model.add(tf.layers.dense({ units: 1, inputShape: [1] }));
  model.compile({ loss: 'meanSquaredError', 
       optimizer: tf.train.sgd(0.1)
  });

  // Reshape inputs to match the model's input.
  const xTensor = xs.reshape([100, 1]);
  const yTensor = ys.reshape([100, 1]);

  // Train the model and log progress.
  console.log("Starting training...");
  const t0 = performance.now();
  await model.fit(xTensor, yTensor, {
    batchSize: 32,
    epochs: 20,
    callbacks: {
      onEpochEnd: (epoch, logs) => {
        console.log(`Epoch ${epoch + 1}: Loss = ${logs.loss.toFixed(4)}`);
      },
    },
  });
  const t1 = performance.now();
  console.log(`Training completed in ${(t1 - t0).toFixed(2)} ms.`);

  // Test the model with some values.
  const testValues = [-0.5, 0.5, 2, -2];
  const testXs = tf.tensor2d(testValues, [testValues.length, 1]);
  const preds = model.predict(testXs);
  const predValues = await preds.data();

  console.log("Test inputs: ", testValues);
  console.log("Predictions: ", predValues);
})();

export const TransformerJS = () => {
  return <View style={{ flex: 1, backgroundColor: "cyan" }} />;
};
