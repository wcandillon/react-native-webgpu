import { client } from "./setup";

// copyExternalImageToTexture with a NativeVideoFrame source renders the frame
// into the destination through an external texture. The test frame is a
// red/green gradient with diagonal stripes in the blue channel:
// r = x * 255 / (w - 1), g = y * 255 / (h - 1), b = (x + y) & 0x20 ? 220 : 30.
type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | { kind: "ok"; pixels: number[] };

const expectClose = (actual: number[], expected: number[]) => {
  expect(actual.length).toBe(expected.length);
  actual.forEach((value, i) => {
    expect(Math.abs(value - expected[i])).toBeLessThanOrEqual(2);
  });
};

describe("copyExternalImageToTexture with a native frame", () => {
  it("copies a frame into a texture", async () => {
    const result = await client.eval<Record<string, never>, EvalResult>(
      ({ device }) => {
        const FEATURE = "rnwebgpu/native-texture";
        if (!device.features.has(FEATURE as GPUFeatureName)) {
          return { kind: "skip", reason: `${FEATURE} not enabled` };
        }
        if (typeof RNWebGPU?.createTestVideoFrame !== "function") {
          return {
            kind: "skip",
            reason: "createTestVideoFrame is unavailable",
          };
        }
        const size = 64;
        const frame = RNWebGPU.createTestVideoFrame(size, size);
        const texture = device.createTexture({
          size: [size, size],
          format: "rgba8unorm",
          usage:
            GPUTextureUsage.RENDER_ATTACHMENT |
            GPUTextureUsage.COPY_SRC |
            GPUTextureUsage.TEXTURE_BINDING,
        });
        try {
          device.queue.copyExternalImageToTexture(
            { source: frame },
            { texture },
            [size, size],
          );
        } catch (e) {
          frame.release();
          return { kind: "fail", reason: `${(e as Error).message ?? e}` };
        }
        // The frame is no longer needed once the copy is issued.
        frame.release();

        const bytesPerRow = 256;
        const buffer = device.createBuffer({
          size: bytesPerRow * size,
          usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
        });
        const encoder = device.createCommandEncoder();
        encoder.copyTextureToBuffer({ texture }, { buffer, bytesPerRow }, [
          size,
          size,
        ]);
        device.queue.submit([encoder.finish()]);
        return buffer.mapAsync(GPUMapMode.READ).then(() => {
          const data = new Uint8Array(buffer.getMappedRange());
          const px = (x: number, y: number) =>
            Array.from(
              data.slice(y * bytesPerRow + x * 4, y * bytesPerRow + x * 4 + 4),
            );
          const pixels = [
            ...px(0, 0),
            ...px(63, 0),
            ...px(0, 63),
            ...px(63, 63),
            ...px(32, 0),
          ];
          buffer.unmap();
          buffer.destroy();
          texture.destroy();
          return { kind: "ok" as const, pixels };
        });
      },
    );
    if (result.kind === "skip") {
      console.warn(`[CopyVideoFrameToTexture] skipped: ${result.reason}`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(result.reason);
    }
    expectClose(result.pixels, [
      ...[0, 0, 30, 255],
      ...[255, 0, 220, 255],
      ...[0, 255, 220, 255],
      ...[255, 255, 220, 255],
      ...[129, 0, 220, 255],
    ]);
  });

  it("copies a flipped sub-region into a region of the texture", async () => {
    const result = await client.eval<Record<string, never>, EvalResult>(
      ({ device }) => {
        const FEATURE = "rnwebgpu/native-texture";
        if (!device.features.has(FEATURE as GPUFeatureName)) {
          return { kind: "skip", reason: `${FEATURE} not enabled` };
        }
        if (typeof RNWebGPU?.createTestVideoFrame !== "function") {
          return {
            kind: "skip",
            reason: "createTestVideoFrame is unavailable",
          };
        }
        const frame = RNWebGPU.createTestVideoFrame(64, 64);
        // A 64x64 destination cleared to blue; the right half of the frame,
        // flipped, lands in the top-left 32x64 quadrant and the rest stays.
        const texture = device.createTexture({
          size: [64, 64],
          format: "bgra8unorm",
          usage:
            GPUTextureUsage.RENDER_ATTACHMENT |
            GPUTextureUsage.COPY_SRC |
            GPUTextureUsage.COPY_DST,
        });
        const blue = new Uint8Array(64 * 64 * 4);
        for (let i = 0; i < blue.length; i += 4) {
          blue[i] = 255; // b
          blue[i + 3] = 255;
        }
        device.queue.writeTexture(
          { texture },
          blue,
          { bytesPerRow: 256 },
          [64, 64],
        );
        try {
          device.queue.copyExternalImageToTexture(
            { source: frame, origin: { x: 32, y: 0 }, flipY: true },
            { texture, origin: { x: 0, y: 0 } },
            [32, 64],
          );
        } catch (e) {
          frame.release();
          return { kind: "fail", reason: `${(e as Error).message ?? e}` };
        }
        frame.release();

        const bytesPerRow = 256;
        const buffer = device.createBuffer({
          size: bytesPerRow * 64,
          usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
        });
        const encoder = device.createCommandEncoder();
        encoder.copyTextureToBuffer(
          { texture },
          { buffer, bytesPerRow },
          [64, 64],
        );
        device.queue.submit([encoder.finish()]);
        return buffer.mapAsync(GPUMapMode.READ).then(() => {
          const data = new Uint8Array(buffer.getMappedRange());
          // bgra8unorm: swap back to rgba for the assertions
          const px = (x: number, y: number) => {
            const o = y * bytesPerRow + x * 4;
            return [data[o + 2], data[o + 1], data[o], data[o + 3]];
          };
          const pixels = [...px(0, 0), ...px(31, 63), ...px(40, 10)];
          buffer.unmap();
          buffer.destroy();
          texture.destroy();
          return { kind: "ok" as const, pixels };
        });
      },
    );
    if (result.kind === "skip") {
      console.warn(`[CopyVideoFrameToTexture] skipped: ${result.reason}`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(result.reason);
    }
    expectClose(result.pixels, [
      // destination (0, 0) is source (32, 63): r = 32 * 255 / 63, b = (95 & 0x20) ? 220 : 30
      ...[129, 255, 30, 255],
      // destination (31, 63) is source (63, 0)
      ...[255, 0, 220, 255],
      // outside the copy region: untouched blue
      ...[0, 0, 255, 255],
    ]);
  });

  it("rejects a destination without RENDER_ATTACHMENT", async () => {
    const result = await client.eval<
      Record<string, never>,
      { kind: "skip"; reason: string } | { kind: "ok"; message: string }
    >(({ device }) => {
      if (typeof RNWebGPU?.createTestVideoFrame !== "function") {
        return { kind: "skip", reason: "createTestVideoFrame is unavailable" };
      }
      const frame = RNWebGPU.createTestVideoFrame(16, 16);
      const texture = device.createTexture({
        size: [16, 16],
        format: "rgba8unorm",
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
      });
      let message = "did not throw";
      try {
        device.queue.copyExternalImageToTexture(
          { source: frame },
          { texture },
          [16, 16],
        );
      } catch (e) {
        message = `${(e as Error).message ?? e}`;
      }
      frame.release();
      texture.destroy();
      return { kind: "ok", message };
    });
    if (result.kind === "skip") {
      console.warn(`[CopyVideoFrameToTexture] skipped: ${result.reason}`);
      return;
    }
    expect(result.message).toContain("RENDER_ATTACHMENT");
  });
});
