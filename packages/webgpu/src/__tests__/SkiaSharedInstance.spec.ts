import { client } from "./setup";

// When react-native-skia is installed, react-native-webgpu adopts Skia's Dawn
// instance instead of creating its own (createInstance in
// cpp/rnwgpu/WebGPURuntime.cpp), so that its async pump serves the device Skia
// renders with: callbacks on a device imported with
// importDevice(Skia.getNativeDevice()) settle. With a second instance they
// never do. This is the end-to-end check of that contract. On Android it
// covers both halves of the lookup: librnskia.so loaded before our library
// (WebGPUModule.java) and found by name (RTLD_DEFAULT never searches it).
//
// iOS is skipped until the example app picks up a react-native-skia release
// with wcandillon/react-native-skia#4306: before it the linker drops the
// getter from the app binary and the lookup fails for another reason.

type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "ok"; bytes: number[]; ms: number };

const EXPECTED = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

describe("Skia shared instance", () => {
  it("settles async work on the device imported from Skia", async () => {
    if (client.OS !== "android") {
      return;
    }
    const result = await client.eval<{ expected: number[] }, EvalResult>(
      ({ skia, expected }) => {
        if (skia === undefined) {
          return { kind: "skip", reason: "the harness exposes no Skia" };
        }
        let pointer: bigint;
        try {
          pointer = skia.getNativeDevice();
        } catch (e) {
          // Skia has no GPU it can render with here: nothing to share.
          return {
            kind: "skip",
            reason: `Skia has no GPU device: ${(e as Error).message}`,
          };
        }
        const device = RNWebGPU.importDevice(pointer);
        const size = expected.length;
        const src = device.createBuffer({
          size,
          usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
        });
        device.queue.writeBuffer(src, 0, new Uint8Array(expected));
        const dst = device.createBuffer({
          size,
          usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
        });
        const encoder = device.createCommandEncoder();
        encoder.copyBufferToBuffer(src, 0, dst, 0, size);
        device.queue.submit([encoder.finish()]);
        const start = Date.now();
        let timer: ReturnType<typeof setTimeout> | undefined;
        const timeout = new Promise<never>((_, reject) => {
          timer = setTimeout(() => {
            reject(
              new Error(
                "mapAsync on the device imported from Skia did not settle " +
                  "within 3000 ms: react-native-webgpu is not pumping Skia's " +
                  "instance",
              ),
            );
          }, 3000);
        });
        return Promise.race([dst.mapAsync(GPUMapMode.READ), timeout]).then(
          () => {
            clearTimeout(timer);
            const bytes = Array.from(new Uint8Array(dst.getMappedRange()));
            dst.unmap();
            return { kind: "ok", bytes, ms: Date.now() - start };
          },
        );
      },
      { expected: EXPECTED },
    );
    if (result.kind === "skip") {
      console.warn(`[SkiaSharedInstance] skipped: ${result.reason}`);
      return;
    }
    expect(result.bytes).toEqual(EXPECTED);
  });
});
