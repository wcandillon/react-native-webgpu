import * as tf from "@tensorflow/tfjs";
import { WebGPUBackend } from "@tensorflow/tfjs-backend-webgpu";

import { PlatformReactNative } from "../Tensorflow/Platform";

// tfjs uses this for fetch / encode / decode / timing in non-browser
// environments. Same Platform impl as the Tensorflow demo.
tf.setPlatform("react-native", new PlatformReactNative());

let current: { device: GPUDevice; ready: Promise<void> } | null = null;

// Runs tfjs's WebGPU backend on the given device instead of one it creates
// itself, so tensors can wrap our buffers with zero copies and results can
// stay on the GPU. Importing the backend package registered a "webgpu"
// factory that would request its own device; it is replaced with one bound
// to ours. Kernels stay registered (removeBackend only runs dispose hooks),
// so binding to a different device later works too, as long as tensors from
// the previous backend are never touched again.
export const ensureTfjsWebGPU = (
  device: GPUDevice,
  adapter: GPUAdapter,
): Promise<void> => {
  if (current && current.device === device) {
    return current.ready;
  }
  const ready = (async () => {
    if (tf.findBackendFactory("webgpu")) {
      tf.removeBackend("webgpu");
    }
    tf.registerBackend(
      "webgpu",
      () => new WebGPUBackend(device, adapter.info),
      2,
    );
    await tf.setBackend("webgpu");
    await tf.ready();
  })();
  current = { device, ready };
  ready.catch(() => {
    if (current?.ready === ready) {
      current = null;
    }
  });
  return ready;
};
