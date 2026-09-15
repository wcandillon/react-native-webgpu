import * as tf from "@tensorflow/tfjs";
import "@tensorflow/tfjs-backend-webgpu";

import { PlatformReactNative } from "../Tensorflow/Platform";

// tfjs uses this for fetch / encode / decode / timing in non-browser
// environments. Same Platform impl as the Tensorflow demo.
tf.setPlatform("react-native", new PlatformReactNative());

let ready: Promise<void> | null = null;

// Selects the tfjs WebGPU backend once for the app lifetime. The backend
// owns its own GPUDevice, separate from the one the camera demos render
// with, which is why model inputs travel through a CPU readback.
export const ensureTfjsWebGPU = (): Promise<void> => {
  if (!ready) {
    ready = (async () => {
      await tf.setBackend("webgpu");
      await tf.ready();
    })().catch((e) => {
      ready = null;
      throw e;
    });
  }
  return ready;
};
