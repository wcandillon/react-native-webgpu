import { installWebGPU } from "react-native-webgpu";
import {
  createWorkletRuntime,
  runOnRuntime,
  scheduleOnRN,
} from "react-native-worklets";

// Test-harness helper (see packages/webgpu ImplicitDeviceSync.spec.ts): uses
// one GPUDevice from two runtimes at once. A dedicated worklet runtime encodes
// and submits copy work while the JS thread keeps creating buffers and writing
// to the queue on the same device. Without Dawn's implicit-device-
// synchronization feature this is a data race inside Dawn; with it (the
// default) both loops must complete cleanly. This must live in app code, not
// in an eval'd test body: the "worklet" directive only works when the babel
// worklets plugin compiles it into the app bundle.
export const workletDeviceStress = (
  device: GPUDevice,
): Promise<{ jsOk: boolean; workletOk: boolean }> => {
  const runtime = createWorkletRuntime({ name: "WebGPUSyncStressRuntime" });
  const workletDone = new Promise<boolean>((resolve) => {
    runOnRuntime(runtime, (dev: GPUDevice) => {
      "worklet";
      installWebGPU();
      let ok = true;
      try {
        for (let i = 0; i < 200; i++) {
          const src = dev.createBuffer({
            size: 256,
            usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
          });
          const dst = dev.createBuffer({
            size: 256,
            usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC,
          });
          const encoder = dev.createCommandEncoder();
          encoder.copyBufferToBuffer(src, 0, dst, 0, 256);
          dev.queue.submit([encoder.finish()]);
          src.destroy();
          dst.destroy();
        }
      } catch {
        ok = false;
      }
      scheduleOnRN(resolve, ok);
    })(device);
  });
  // Hammer the same device from the JS runtime in parallel.
  let jsOk = true;
  try {
    for (let i = 0; i < 200; i++) {
      const buffer = device.createBuffer({
        size: 1024,
        usage: GPUBufferUsage.COPY_DST,
      });
      device.queue.writeBuffer(buffer, 0, new Uint8Array(1024));
      buffer.destroy();
    }
  } catch {
    jsOk = false;
  }
  return workletDone.then((workletOk) => ({ jsOk, workletOk }));
};
