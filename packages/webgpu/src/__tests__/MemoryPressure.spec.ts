/* eslint-disable @typescript-eslint/no-explicit-any */
import { client } from "./setup";

// HermesInternal.getInstrumentedStats().js_externalBytes is the memory the
// runtime has been told about through setExternalMemoryPressure. It only
// exists on Hermes, so these tests measure on device and return null (which
// skips the assertions) on the web reference and node runners.
describe("Native object memory pressure", () => {
  it("charges the size of a buffer to the Hermes GC", async () => {
    const result = await client.eval<
      Record<string, unknown>,
      { delta: number; size: number } | null
    >(({ device }) => {
      const hermes = (globalThis as any).HermesInternal;
      if (!hermes?.getInstrumentedStats) {
        return null;
      }
      const externalBytes = (): number =>
        hermes.getInstrumentedStats().js_externalBytes;
      const size = 16 * 1024 * 1024;
      // A GC between the two readings frees the memory of earlier tests and
      // skews the delta, so retry a few times and keep the exact match.
      let delta = 0;
      for (let attempt = 0; attempt < 5; attempt++) {
        const before = externalBytes();
        const buffer = device.createBuffer({
          size,
          usage: GPUBufferUsage.VERTEX,
        });
        delta = externalBytes() - before;
        buffer.destroy();
        if (delta === size) {
          break;
        }
      }
      return { delta, size };
    });
    if (result === null) {
      return;
    }
    expect(result.delta).toBe(result.size);
  });

  it("hands back the same wrapper when an object is unboxed again", async () => {
    const result = await client.eval<
      Record<string, unknown>,
      { sameObject: boolean; delta: number } | null
    >(({ device }) => {
      const hermes = (globalThis as any).HermesInternal;
      const box = (globalThis as any).__webgpuBox;
      if (!hermes?.getInstrumentedStats || typeof box !== "function") {
        return null;
      }
      const externalBytes = (): number =>
        hermes.getInstrumentedStats().js_externalBytes;
      const buffer = device.createBuffer({
        size: 1024 * 1024,
        usage: GPUBufferUsage.VERTEX,
      });
      // __webgpuBox()/unbox() is how worklets move an object to another
      // runtime. Unboxing on the runtime that already has a wrapper must
      // resolve to that wrapper, and charge nothing more.
      const boxed = box(buffer);
      const before = externalBytes();
      let sameObject = true;
      for (let i = 0; i < 100; i++) {
        if (boxed.unbox() !== buffer) {
          sameObject = false;
        }
      }
      const delta = externalBytes() - before;
      buffer.destroy();
      return { sameObject, delta };
    });
    if (result === null) {
      return;
    }
    expect(result.sameObject).toBe(true);
    // A GC in between can only lower the reading.
    expect(result.delta).toBeLessThanOrEqual(0);
  });
});
