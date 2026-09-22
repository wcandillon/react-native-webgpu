import { client } from "./setup";

// The harness mounts a 64x64pt probe view made of four solid quadrants
// (apps/example/src/Tests.tsx): red top-left, lime top-right, blue
// bottom-left, white bottom-right. Each eval body is serialized, so the
// readback helper is repeated inline.

type RGBA = [number, number, number, number];

const RED: RGBA = [255, 0, 0, 255];
const LIME: RGBA = [0, 255, 0, 255];
const BLUE: RGBA = [0, 0, 255, 255];
const WHITE: RGBA = [255, 255, 255, 255];

const expectColor = (actual: number[], expected: RGBA, tolerance = 3) => {
  expected.forEach((channel, i) => {
    expect(Math.abs(actual[i] - channel)).toBeLessThanOrEqual(tolerance);
  });
};

const nativeOnly = () => client.OS === "ios" || client.OS === "android";

describe("drawElementImageToTexture", () => {
  it("draws a native view at its natural pixel size", async () => {
    if (!nativeOnly()) {
      return;
    }
    const result = await client.eval(({ device, views }) => {
      const size = Math.round(views!.probeSize * views!.pixelRatio);
      const texture = device.createTexture({
        size: [size, size],
        format: "rgba8unorm",
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC,
      });
      const bytesPerRow = Math.ceil((size * 4) / 256) * 256;
      const output = device.createBuffer({
        size: bytesPerRow * size,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
      });
      return device.queue
        .drawElementImageToTexture({ source: views!.probe! }, { texture })
        .then(() => {
          const encoder = device.createCommandEncoder();
          encoder.copyTextureToBuffer(
            { texture },
            { buffer: output, bytesPerRow },
            [size, size],
          );
          device.queue.submit([encoder.finish()]);
          return output.mapAsync(GPUMapMode.READ);
        })
        .then(() => {
          const data = new Uint8Array(output.getMappedRange());
          const at = (x: number, y: number) =>
            Array.from(
              data.slice(y * bytesPerRow + x * 4, y * bytesPerRow + x * 4 + 4),
            );
          const q = size / 4;
          const sample = {
            size,
            topLeft: at(q, q),
            topRight: at(3 * q, q),
            bottomLeft: at(q, 3 * q),
            bottomRight: at(3 * q, 3 * q),
          };
          output.unmap();
          return sample;
        });
    });
    expect(result.size).toBeGreaterThan(0);
    expectColor(result.topLeft, RED);
    expectColor(result.topRight, LIME);
    expectColor(result.bottomLeft, BLUE);
    expectColor(result.bottomRight, WHITE);
  });

  it("scales the view to destination.size and honours origin", async () => {
    if (!nativeOnly()) {
      return;
    }
    const result = await client.eval(({ device, views }) => {
      // 64x64 texture, view drawn scaled into the 32x32 bottom-right corner;
      // the rest is cleared to a known color via writeTexture first.
      const size = 64;
      const texture = device.createTexture({
        size: [size, size],
        format: "rgba8unorm",
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC,
      });
      const clear = new Uint8Array(size * size * 4);
      for (let i = 0; i < clear.length; i += 4) {
        clear[i] = 9;
        clear[i + 1] = 8;
        clear[i + 2] = 7;
        clear[i + 3] = 255;
      }
      device.queue.writeTexture({ texture }, clear, { bytesPerRow: size * 4 }, [
        size,
        size,
      ]);
      const bytesPerRow = 256;
      const output = device.createBuffer({
        size: bytesPerRow * size,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
      });
      return device.queue
        .drawElementImageToTexture(
          { source: views!.probe! },
          { texture, origin: [32, 32], size: [32, 32] },
        )
        .then(() => {
          const encoder = device.createCommandEncoder();
          encoder.copyTextureToBuffer(
            { texture },
            { buffer: output, bytesPerRow },
            [size, size],
          );
          device.queue.submit([encoder.finish()]);
          return output.mapAsync(GPUMapMode.READ);
        })
        .then(() => {
          const data = new Uint8Array(output.getMappedRange());
          const at = (x: number, y: number) =>
            Array.from(
              data.slice(y * bytesPerRow + x * 4, y * bytesPerRow + x * 4 + 4),
            );
          const sample = {
            untouched: at(8, 8),
            topLeft: at(40, 40),
            topRight: at(56, 40),
            bottomLeft: at(40, 56),
            bottomRight: at(56, 56),
          };
          output.unmap();
          return sample;
        });
    });
    expectColor(result.untouched, [9, 8, 7, 255], 0);
    expectColor(result.topLeft, RED);
    expectColor(result.topRight, LIME);
    expectColor(result.bottomLeft, BLUE);
    expectColor(result.bottomRight, WHITE);
  });

  it("crops to the source rectangle and swizzles into bgra8unorm", async () => {
    if (!nativeOnly()) {
      return;
    }
    const result = await client.eval(({ device, views }) => {
      // The top-right quadrant only (lime), drawn into a 16x16 bgra8unorm
      // texture: every pixel must read back as lime in BGRA byte order.
      const half = views!.probeSize / 2;
      const size = 16;
      const texture = device.createTexture({
        size: [size, size],
        format: "bgra8unorm",
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC,
      });
      const bytesPerRow = 256;
      const output = device.createBuffer({
        size: bytesPerRow * size,
        usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
      });
      return device.queue
        .drawElementImageToTexture(
          {
            source: views!.probe!,
            sourceX: half,
            sourceY: 0,
            sourceWidth: half,
            sourceHeight: half,
          },
          { texture, size: [size, size] },
        )
        .then(() => {
          const encoder = device.createCommandEncoder();
          encoder.copyTextureToBuffer(
            { texture },
            { buffer: output, bytesPerRow },
            [size, size],
          );
          device.queue.submit([encoder.finish()]);
          return output.mapAsync(GPUMapMode.READ);
        })
        .then(() => {
          const data = new Uint8Array(output.getMappedRange());
          const at = (x: number, y: number) =>
            Array.from(
              data.slice(y * bytesPerRow + x * 4, y * bytesPerRow + x * 4 + 4),
            );
          const sample = {
            first: at(1, 1),
            center: at(8, 8),
            last: at(14, 14),
          };
          output.unmap();
          return sample;
        });
    });
    // bgra8unorm read back: bytes are B, G, R, A.
    const limeBGRA: RGBA = [0, 255, 0, 255];
    expectColor(result.first, limeBGRA);
    expectColor(result.center, limeBGRA);
    expectColor(result.last, limeBGRA);
  });

  it("rejects when the natural size does not fit the texture", async () => {
    if (!nativeOnly()) {
      return;
    }
    const result = await client.eval(({ device, views }) => {
      const texture = device.createTexture({
        size: [8, 8],
        format: "rgba8unorm",
        usage: GPUTextureUsage.COPY_DST,
      });
      return device.queue
        .drawElementImageToTexture({ source: views!.probe! }, { texture })
        .then(
          () => "resolved",
          (e: Error) => e.message,
        );
    });
    expect(result).toContain("does not fit");
  });

  it("rejects an unknown view and an unsupported format", async () => {
    if (!nativeOnly()) {
      return;
    }
    const result = await client.eval(({ device }) => {
      const texture = device.createTexture({
        size: [8, 8],
        format: "rgba8unorm",
        usage: GPUTextureUsage.COPY_DST,
      });
      const floatTexture = device.createTexture({
        size: [8, 8],
        format: "rgba16float",
        usage: GPUTextureUsage.COPY_DST,
      });
      const unknownView = device.queue
        .drawElementImageToTexture({ source: 987654321 }, { texture })
        .then(
          () => "resolved",
          (e: Error) => e.message,
        );
      let unsupportedFormat: string;
      try {
        device.queue.drawElementImageToTexture(
          { source: 987654321 },
          { texture: floatTexture },
        );
        unsupportedFormat = "no throw";
      } catch (e) {
        unsupportedFormat = (e as Error).message;
      }
      return unknownView.then((message) => ({
        unknownView: message,
        unsupportedFormat,
      }));
    });
    expect(result.unknownView).toContain("no native view found");
    expect(result.unsupportedFormat).toContain("rgba8unorm");
  });
});
