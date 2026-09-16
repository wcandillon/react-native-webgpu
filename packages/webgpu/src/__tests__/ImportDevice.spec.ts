import { client } from "./setup";

// importDevice() wraps an externally created WGPUDevice. The only exporter we
// can exercise end to end is a Graphite build of @shopify/react-native-skia
// (Skia.getNativeDevice()), so these tests run on the native app harness and
// skip themselves when the installed Skia is a default (Ganesh) build.
describe("importDevice", () => {
  const isNative = () => client.OS === "ios" || client.OS === "android";

  it("returns the same GPUDevice for the same pointer (idempotent)", async () => {
    if (!isNative()) {
      return;
    }
    const result = await client.eval(({ importDevice, Skia }) => {
      if (
        !importDevice ||
        !Skia ||
        typeof Skia.getNativeDevice !== "function"
      ) {
        return { skipped: true };
      }
      const pointer = Skia.getNativeDevice();
      const a = importDevice(pointer);
      const b = importDevice(pointer);
      const c = importDevice(Skia.getNativeDevice());
      return {
        skipped: false,
        samePointer: pointer === Skia.getNativeDevice(),
        sameAB: a === b,
        sameAC: a === c,
        isDevice: a.__brand === "GPUDevice",
        label: a.label,
      };
    });
    if (result.skipped) {
      console.warn("importDevice: skipped (Skia is not a Graphite build)");
      return;
    }
    expect(result.samePointer).toBe(true);
    expect(result.sameAB).toBe(true);
    expect(result.sameAC).toBe(true);
    expect(result.isDevice).toBe(true);
    expect(result.label).toBe("Imported Device");
  });

  it("shares state across imports (label, event listeners)", async () => {
    if (!isNative()) {
      return;
    }
    const result = await client.eval(({ importDevice, Skia }) => {
      if (
        !importDevice ||
        !Skia ||
        typeof Skia.getNativeDevice !== "function"
      ) {
        return { skipped: true };
      }
      const pointer = Skia.getNativeDevice();
      const a = importDevice(pointer);
      const previous = a.label;
      a.label = "Skia device";
      const b = importDevice(pointer);
      const seenThroughB = b.label;
      // Restore so other tests observe the default label.
      b.label = previous;
      return { skipped: false, seenThroughB, restored: a.label };
    });
    if (result.skipped) {
      return;
    }
    expect(result.seenThroughB).toBe("Skia device");
    expect(result.restored).toBe("Imported Device");
  });

  it("is a working device: async work settles on the shared instance", async () => {
    if (!isNative()) {
      return;
    }
    // Note: eval callbacks must not be `async`. Babel rewrites async arrows
    // into a wrapper that references a hoisted helper (`_refN`), and
    // fn.toString() only ships the wrapper to the device, so the eval fails
    // with "Property '_refN' doesn't exist". Return a promise chain instead.
    const result = await client.eval(({ importDevice, Skia }) => {
      if (
        !importDevice ||
        !Skia ||
        typeof Skia.getNativeDevice !== "function"
      ) {
        return { skipped: true, data: [] as number[] };
      }
      const device = importDevice(Skia.getNativeDevice());
      const src = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
      });
      const dst = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
      });
      device.queue.writeBuffer(src, 0, new Uint32Array([1, 2, 3, 4]));
      const encoder = device.createCommandEncoder();
      encoder.copyBufferToBuffer(src, 0, dst, 0, 16);
      device.queue.submit([encoder.finish()]);
      return dst.mapAsync(GPUMapMode.READ).then(() => {
        const data = Array.from(new Uint32Array(dst.getMappedRange()));
        dst.unmap();
        src.destroy();
        dst.destroy();
        return { skipped: false, data };
      });
    });
    if (result.skipped) {
      return;
    }
    expect(result.data).toEqual([1, 2, 3, 4]);
  });

  it("rejects a null pointer", async () => {
    if (!isNative()) {
      return;
    }
    const result = await client.eval(({ importDevice }) => {
      if (!importDevice) {
        return { skipped: true };
      }
      try {
        importDevice(BigInt(0));
        return { skipped: false, threw: false, message: "" };
      } catch (e) {
        return {
          skipped: false,
          threw: true,
          message: e instanceof Error ? e.message : String(e),
        };
      }
    });
    if (result.skipped) {
      return;
    }
    expect(result.threw).toBe(true);
    expect(result.message).toContain("non-null WGPUDevice pointer");
  });
});
