import { client } from "./setup";

// A pixel as [r, g, b, a], 0..255.
type Rgba = number[];

// The middle of the left and right halves of the test frame.
type Halves = { left: Rgba; right: Rgba };

type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | {
      kind: "ok";
      withoutColorSpace: Halves;
      withColorSpace: Halves;
      withSourceColorSpace: Halves;
    };

// The test frame is red on the left and green on the right, encoded in
// BT.709 narrow range, but its AHardwareBuffer does not say so: wrapped
// without a colorSpace it is read with the platform's guess, as a buffer
// from another library is.
const TEST_FRAME_COLOR_SPACE = { matrix: "rec709", range: "narrow" } as const;
const TEST_FRAME_COLORS: Halves = {
  left: [255, 0, 0, 255],
  right: [0, 255, 0, 255],
};

// Within 8 levels per channel; on a mismatch, Jest shows both pixels.
const expectColors = (actual: Halves, expected: Halves) => {
  const close = (a: Rgba, b: Rgba) =>
    a.every((channel, i) => Math.abs(channel - b[i]) <= 8);
  if (
    !close(actual.left, expected.left) ||
    !close(actual.right, expected.right)
  ) {
    expect(actual).toEqual(expected);
  }
};

describe("createNativeBufferFromPointer colorSpace", () => {
  // Android only: the pointer is the test frame's AHardwareBuffer*. On Apple
  // platforms nothing in JS gives a CVPixelBufferRef to wrap.
  it("reads a buffer without color metadata with the colorSpace it is given", async () => {
    const result = await client.eval<
      { testFrameColorSpace: typeof TEST_FRAME_COLOR_SPACE },
      EvalResult
    >(
      ({ device, testFrameColorSpace }) => {
        if (
          !device.features.has(
            "shared-texture-memory-ahardware-buffer" as GPUFeatureName,
          )
        ) {
          return {
            kind: "skip",
            reason: "the pointer of a test frame is an AHardwareBuffer only",
          };
        }
        if (
          !device.features.has(
            "opaque-ycbcr-android-for-external-texture" as GPUFeatureName,
          )
        ) {
          return {
            kind: "skip",
            reason: "the device cannot sample YUV hardware buffers",
          };
        }

        let testFrame: ReturnType<typeof RNWebGPU.createTestVideoFrame>;
        try {
          testFrame = RNWebGPU.createTestVideoFrame(64, 64, "nv12");
        } catch (e) {
          const message = `${(e as Error).message ?? e}`;
          // Android before API 29 cannot allocate or fill the YUV layout.
          if (message.indexOf("unsupported") !== -1) {
            return { kind: "skip", reason: message };
          }
          return { kind: "fail", reason: message };
        }
        const pointer = testFrame.handle;
        const wrappedWithoutColorSpace =
          RNWebGPU.createNativeBufferFromPointer(pointer);
        const wrappedWithColorSpace = RNWebGPU.createNativeBufferFromPointer(
          pointer,
          { colorSpace: testFrameColorSpace },
        );
        const releaseFrames = () => {
          testFrame.release();
          wrappedWithoutColorSpace.release();
          wrappedWithColorSpace.release();
        };

        try {
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
              targets: [{ format: "rgba8unorm" }],
            },
            primitive: { topology: "triangle-list" },
          });
          const sampler = device.createSampler({
            magFilter: "linear",
            minFilter: "linear",
          });

          const imports = {
            withoutColorSpace: { source: wrappedWithoutColorSpace },
            withColorSpace: { source: wrappedWithColorSpace },
            withSourceColorSpace: {
              source: wrappedWithoutColorSpace,
              sourceColorSpace: testFrameColorSpace,
            },
          };
          const names = Object.keys(imports) as (keyof typeof imports)[];
          // One 256-byte row (the copy alignment) per import.
          const ROW = 256;
          const readback = device.createBuffer({
            size: ROW * names.length,
            usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
          });
          const encoder = device.createCommandEncoder();
          const externalTextures = names.map((name, row) => {
            const externalTexture = device.importExternalTexture(imports[name]);
            // Two pixels, whose centers sample the middle of each half.
            const halves = device.createTexture({
              size: [2, 1],
              format: "rgba8unorm",
              usage:
                GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
            });
            const pass = encoder.beginRenderPass({
              colorAttachments: [
                {
                  view: halves.createView(),
                  clearValue: { r: 0, g: 0, b: 0, a: 1 },
                  loadOp: "clear",
                  storeOp: "store",
                },
              ],
            });
            pass.setPipeline(pipeline);
            pass.setBindGroup(
              0,
              device.createBindGroup({
                layout: pipeline.getBindGroupLayout(0),
                entries: [
                  { binding: 0, resource: externalTexture },
                  { binding: 1, resource: sampler },
                ],
              }),
            );
            pass.draw(3);
            pass.end();
            encoder.copyTextureToBuffer(
              { texture: halves },
              { buffer: readback, offset: ROW * row, bytesPerRow: ROW },
              [2, 1],
            );
            return externalTexture;
          });
          device.queue.submit([encoder.finish()]);
          externalTextures.forEach((externalTexture) =>
            externalTexture.destroy(),
          );

          return readback
            .mapAsync(GPUMapMode.READ)
            .then(() => {
              const bytes = new Uint8Array(readback.getMappedRange());
              const halvesOf = (name: keyof typeof imports): Halves => {
                const start = ROW * names.indexOf(name);
                return {
                  left: Array.from(bytes.subarray(start, start + 4)),
                  right: Array.from(bytes.subarray(start + 4, start + 8)),
                };
              };
              return {
                kind: "ok" as const,
                withoutColorSpace: halvesOf("withoutColorSpace"),
                withColorSpace: halvesOf("withColorSpace"),
                withSourceColorSpace: halvesOf("withSourceColorSpace"),
              };
            })
            .finally(releaseFrames);
        } catch (e) {
          releaseFrames();
          return { kind: "fail", reason: `${(e as Error).message ?? e}` };
        }
      },
      { testFrameColorSpace: TEST_FRAME_COLOR_SPACE },
    );

    if (result.kind === "skip") {
      console.log(`NativeBufferColorSpace: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`NativeBufferColorSpace: ${result.reason}`);
    }
    console.log(`NativeBufferColorSpace: ${JSON.stringify(result)}`);

    // withoutColorSpace is logged, not checked: its colors are off only where
    // the platform's guess is not BT.709 (it is BT.601 when the driver has
    // none).
    expectColors(result.withColorSpace, TEST_FRAME_COLORS);
    expect(result.withColorSpace).toEqual(result.withSourceColorSpace);
  });
});
