import { client } from "./setup";

// A pixel as [r, g, b], 0..255.
type Rgb = number[];

type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | { kind: "ok"; left: Rgb; right: Rgb };

// The colorSpace option of createNativeBufferFromPointer, as a WebCodecs
// VideoColorSpaceInit. `undefined` passes no options at all.
type ColorSpaceInit = {
  primaries?: string;
  transfer?: string;
  matrix?: string;
  fullRange?: boolean;
};

// The samples of the NV12 test frame (test_yuv_frame in PlatformContext.h):
// red on the left half, green on the right, encoded as BT.709 limited range.
const RED = [63, 102, 240];
const GREEN = [173, 42, 26];

// The luma weights (Kr, Kb) of the YCbCr matrices.
const MATRICES = {
  bt709: [0.2126, 0.0722],
  smpte170m: [0.299, 0.114],
} as const;

// What the external texture gives for a [Y, Cb, Cr] sample read with `matrix`
// and `fullRange`, with the BT.709 primaries and transfer function the buffer
// declares, on an sRGB canvas: YCbCr to R'G'B', the BT.709 OETF undone, the
// sRGB one applied. The primaries are the same, so no gamut conversion.
const expectedRgb = (
  [y8, cb8, cr8]: number[],
  matrix: keyof typeof MATRICES,
  fullRange: boolean,
): Rgb => {
  const [kr, kb] = MATRICES[matrix];
  const kg = 1 - kr - kb;
  const y = fullRange ? y8 / 255 : (y8 - 16) / 219;
  const cb = fullRange ? (cb8 - 128) / 255 : (cb8 - 128) / 224;
  const cr = fullRange ? (cr8 - 128) / 255 : (cr8 - 128) / 224;
  const r = y + 2 * (1 - kr) * cr;
  const b = y + 2 * (1 - kb) * cb;
  const g = (y - kr * r - kb * b) / kg;
  return [r, g, b].map((v) => {
    const c = Math.min(Math.max(v, 0), 1);
    const linear =
      c < 0.081 ? c / 4.5 : Math.pow((c + 0.099) / 1.099, 1 / 0.45);
    const srgb =
      linear <= 0.0031308
        ? 12.92 * linear
        : 1.055 * Math.pow(linear, 1 / 2.4) - 0.055;
    return Math.round(srgb * 255);
  });
};

// Wraps the test frame's own pointer (a CVPixelBufferRef carrying BT.709
// limited range attachments on Apple platforms) with
// createNativeBufferFromPointer, passing `colorSpace` when it is set, and
// samples the result through importExternalTexture onto the canvas.
const render = (colorSpace: ColorSpaceInit | undefined) =>
  client.eval<{ colorSpace: ColorSpaceInit | null }, EvalResult>(
    ({ device, gpu, ctx, canvas, colorSpace: init }) => {
      if (
        !device.features.has(
          "shared-texture-memory-iosurface" as GPUFeatureName,
        )
      ) {
        // Android reads a YUV AHardwareBuffer through Vulkan's YCbCr
        // sampler, whose results these expectations do not model.
        return {
          kind: "skip",
          reason: "the expectations are for CVPixelBuffer (Apple platforms)",
        };
      }
      const rnwgpu = RNWebGPU as unknown as {
        createTestPixelBuffer?: (
          width: number,
          height: number,
        ) => { handle: bigint; release(): void };
      };
      if (typeof rnwgpu.createTestPixelBuffer !== "function") {
        return {
          kind: "skip",
          reason: "RNWebGPU.createTestPixelBuffer is unavailable",
        };
      }

      let pixelBuffer: { handle: bigint; release(): void };
      let frame: ReturnType<typeof RNWebGPU.createNativeBufferFromPointer>;
      try {
        pixelBuffer = rnwgpu.createTestPixelBuffer(64, 64);
        frame =
          init === null
            ? RNWebGPU.createNativeBufferFromPointer(pixelBuffer.handle)
            : RNWebGPU.createNativeBufferFromPointer(pixelBuffer.handle, {
                colorSpace: init,
              });
        // The frame holds its own reference.
        pixelBuffer.release();
      } catch (e) {
        return { kind: "fail", reason: `${(e as Error).message ?? e}` };
      }

      try {
        const externalTexture = device.importExternalTexture({
          source: frame,
          label: "yuv-pointer-frame",
        });
        const module = device.createShaderModule({
          code: /* wgsl */ `
            struct VsOut {
              @builtin(position) position: vec4f,
              @location(0) uv: vec2f,
            };

            @vertex fn vs(@builtin(vertex_index) vid: u32) -> VsOut {
              var positions = array<vec2f, 3>(
                vec2f(-1.0, -3.0),
                vec2f(-1.0,  1.0),
                vec2f( 3.0,  1.0),
              );
              var uvs = array<vec2f, 3>(
                vec2f(0.0, 2.0),
                vec2f(0.0, 0.0),
                vec2f(2.0, 0.0),
              );
              var out: VsOut;
              out.position = vec4f(positions[vid], 0.0, 1.0);
              out.uv = uvs[vid];
              return out;
            }

            @group(0) @binding(0) var srcTex: texture_external;
            @group(0) @binding(1) var srcSampler: sampler;

            @fragment fn fs(in: VsOut) -> @location(0) vec4f {
              return textureSampleBaseClampToEdge(srcTex, srcSampler, in.uv);
            }
          `,
        });
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: { module, entryPoint: "vs" },
          fragment: {
            module,
            entryPoint: "fs",
            targets: [{ format: gpu.getPreferredCanvasFormat() }],
          },
          primitive: { topology: "triangle-list" },
        });
        const sampler = device.createSampler({
          magFilter: "linear",
          minFilter: "linear",
        });
        const bindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [
            { binding: 0, resource: externalTexture },
            { binding: 1, resource: sampler },
          ],
        });

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: ctx.getCurrentTexture().createView(),
              clearValue: { r: 0, g: 0, b: 0, a: 1 },
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        });
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, bindGroup);
        pass.draw(3);
        pass.end();
        device.queue.submit([encoder.finish()]);
        externalTexture.destroy();

        return canvas
          .getImageData()
          .then(
            (image: {
              data: number[];
              width: number;
              height: number;
              format: string;
            }) => {
              frame.release();
              const bgra = image.format === "bgra8unorm";
              const pixel = (x: number, y: number): Rgb => {
                const i = (y * image.width + x) * 4;
                return bgra
                  ? [image.data[i + 2], image.data[i + 1], image.data[i]]
                  : [image.data[i], image.data[i + 1], image.data[i + 2]];
              };
              const midY = Math.floor(image.height / 2);
              return {
                kind: "ok" as const,
                left: pixel(Math.floor(image.width / 4), midY),
                right: pixel(Math.floor((image.width * 3) / 4), midY),
              };
            },
          );
      } catch (e) {
        frame.release();
        return { kind: "fail", reason: `${(e as Error).message ?? e}` };
      }
    },
    { colorSpace: colorSpace ?? null },
  );

// 8-bit YCbCr quantization, the bilinear sample and the transfer function
// round trip leave a few levels of slack.
const TOLERANCE = 6;

const expectColor = (actual: Rgb, expected: Rgb) => {
  expected.forEach((channel, i) => {
    expect(Math.abs(actual[i] - channel)).toBeLessThanOrEqual(TOLERANCE);
  });
};

// The largest per-channel difference between two colors.
const distance = (a: Rgb, b: Rgb) =>
  Math.max(...a.map((channel, i) => Math.abs(channel - b[i])));

const run = async (colorSpace: ColorSpaceInit | undefined) => {
  const result = await render(colorSpace);
  if (result.kind === "skip") {
    console.log(`NativeBufferColorSpace: skipping (${result.reason})`);
    return null;
  }
  if (result.kind === "fail") {
    throw new Error(`NativeBufferColorSpace: ${result.reason}`);
  }
  return result;
};

describe("createNativeBufferFromPointer colorSpace", () => {
  it("reads the buffer's own attachments without options", async () => {
    const result = await run(undefined);
    if (!result) {
      return;
    }
    expectColor(result.left, expectedRgb(RED, "bt709", false));
    expectColor(result.right, expectedRgb(GREEN, "bt709", false));
    expectColor(result.left, [255, 0, 0]);
    expectColor(result.right, [0, 255, 0]);
  });

  it("keeps the attachments for the fields an empty colorSpace leaves out", async () => {
    const result = await run({});
    if (!result) {
      return;
    }
    expectColor(result.left, [255, 0, 0]);
    expectColor(result.right, [0, 255, 0]);
  });

  it('reads the samples with the BT.709 matrix for matrix: "bt709"', async () => {
    const result = await run({ matrix: "bt709" });
    if (!result) {
      return;
    }
    expectColor(result.left, expectedRgb(RED, "bt709", false));
    expectColor(result.right, expectedRgb(GREEN, "bt709", false));
  });

  it('reads the samples with the BT.601 matrix for matrix: "smpte170m"', async () => {
    const result = await run({ matrix: "smpte170m" });
    if (!result) {
      return;
    }
    const red = expectedRgb(RED, "smpte170m", false);
    const green = expectedRgb(GREEN, "smpte170m", false);
    expectColor(result.left, red);
    expectColor(result.right, green);
    // The option really changes the result: far from the BT.709 colors.
    expect(distance(red, [255, 0, 0])).toBeGreaterThan(3 * TOLERANCE);
    expect(distance(result.left, [255, 0, 0])).toBeGreaterThan(2 * TOLERANCE);
  });

  it("reads the samples as full range for fullRange: true", async () => {
    const result = await run({ fullRange: true });
    if (!result) {
      return;
    }
    const red = expectedRgb(RED, "bt709", true);
    const green = expectedRgb(GREEN, "bt709", true);
    expectColor(result.left, red);
    expectColor(result.right, green);
    expect(distance(red, [255, 0, 0])).toBeGreaterThan(3 * TOLERANCE);
    expect(distance(result.left, [255, 0, 0])).toBeGreaterThan(2 * TOLERANCE);
  });

  it("reads the samples as limited range for fullRange: false", async () => {
    // The buffer is limited range already, so the colors stay put; the case
    // checks that false is read as limited and not as "unset" or full.
    const result = await run({ fullRange: false });
    if (!result) {
      return;
    }
    expectColor(result.left, [255, 0, 0]);
    expectColor(result.right, [0, 255, 0]);
  });

  it("combines matrix and fullRange", async () => {
    const result = await run({ matrix: "smpte170m", fullRange: true });
    if (!result) {
      return;
    }
    expectColor(result.left, expectedRgb(RED, "smpte170m", true));
    expectColor(result.right, expectedRgb(GREEN, "smpte170m", true));
  });
});
