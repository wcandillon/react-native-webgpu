import { execFile } from "child_process";

import { client } from "./setup";

// The example app's package and activity (apps/example/app.json).
const ANDROID_ACTIVITY =
  "com.reactnativewebgpu.example/com.microsoft.reacttestapp.MainActivity";

const adb = (...args: string[]) =>
  new Promise<void>((resolve, reject) => {
    execFile("adb", args, (error) => (error ? reject(error) : resolve()));
  });

const wait = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

// Sends the example app to the background and brings it back. Hiding its UI
// has Android ask it to give back memory (TRIM_MEMORY_UI_HIDDEN), and
// WebGPUModule then has Dawn trim every live device (see
// GPUDevice::reduceMemoryUsage) on the UI thread, in passes 250ms apart.
// `am send-trim-memory` would be more direct, but Android refuses a
// background level for a process in the foreground, which the app under test
// is.
const hideAndShowApp = async () => {
  await adb("shell", "input", "keyevent", "KEYCODE_HOME");
  await wait(1500);
  await adb("shell", "am", "start", "-n", ANDROID_ACTIVITY);
};

// Uploads through Dawn's staging ring buffers (writeBuffer) and reads the last
// upload back, for durationMs. Run while the UI thread trims the same device,
// it has the trim free those ring buffers under uploads still in flight.
const uploadAndReadBack = (durationMs: number) =>
  client.eval<
    { durationMs: number },
    { rounds: number; mismatches: number; last: number[] }
  >(
    ({ device, durationMs: duration }) => {
      const size = 1024 * 1024;
      const target = device.createBuffer({
        size,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.COPY_SRC,
      });
      const readback = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
      });
      const data = new Uint32Array(size / 4);
      const end = Date.now() + duration;
      let rounds = 0;
      let mismatches = 0;
      let last: number[] = [];
      const round = (): Promise<void> => {
        rounds++;
        data.fill(rounds);
        device.queue.writeBuffer(target, 0, data);
        const encoder = device.createCommandEncoder();
        encoder.copyBufferToBuffer(target, size - 16, readback, 0, 16);
        device.queue.submit([encoder.finish()]);
        return readback.mapAsync(GPUMapMode.READ).then(() => {
          last = Array.from(new Uint32Array(readback.getMappedRange()));
          readback.unmap();
          if (last.some((value) => value !== rounds)) {
            mismatches++;
          }
          return Date.now() < end ? round() : undefined;
        });
      };
      return round().then(() => {
        target.destroy();
        readback.destroy();
        return { rounds, mismatches, last };
      });
    },
    { durationMs },
  );

describe("Memory trim", () => {
  it("keeps a device in use working while Android trims memory", async () => {
    if (client.OS !== "android") {
      return;
    }
    // Uploads before, during and after the trim's passes (about one second).
    const uploads = uploadAndReadBack(3000);
    await hideAndShowApp();
    const during = await uploads;
    expect(during.rounds).toBeGreaterThan(1);
    expect(during.mismatches).toBe(0);
    expect(during.last).toEqual([
      during.rounds,
      during.rounds,
      during.rounds,
      during.rounds,
    ]);

    // Once the trim is over, the device still uploads and reads back.
    const after = await uploadAndReadBack(0);
    expect(after).toEqual({ rounds: 1, mismatches: 0, last: [1, 1, 1, 1] });
  });
});
