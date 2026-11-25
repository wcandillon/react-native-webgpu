import { client } from "./setup";

// A pixel as [r, g, b, a], 0..255.
type Rgba = number[];

type Samples = { left: Rgba; right: Rgba };

type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | { kind: "ok"; plain: Samples; wrapped: Samples; imported: Samples };

// How the test frame is encoded: red on the left half, green on the right, in
// BT.709 narrow range. Its AHardwareBuffer carries no color metadata, so
// wrapped without options it is read with the platform's guess (BT.601
// narrow when the driver has none), as a buffer from another library is.
const OVERRIDE = { matrix: "rec709", range: "narrow" } as const;

const expectColor = (actual: Rgba, expected: Rgba) => {
  expected.forEach((channel, i) => {
    expect(Math.abs(actual[i] - channel)).toBeLessThanOrEqual(8);
  });
};

describe("createNativeBufferFromPointer colorSpace", () => {
  // Wrapping with the option must read the buffer with the color space it is
  // given, as importExternalTexture's sourceColorSpace does. Android only: the
  // pointer is the test frame's AHardwareBuffer*, and on Apple platforms
  // nothing in JS gives a CVPixelBufferRef to wrap.
  it("reads a buffer without color metadata with the color space it is given", async () => {
    const result = await client.eval<{ override: typeof OVERRIDE }, EvalResult>(
      ({ device, override }) => {
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

        let source: ReturnType<typeof RNWebGPU.createTestVideoFrame>;
        try {
          source = RNWebGPU.createTestVideoFrame(64, 64, "nv12");
        } catch (e) {
          const message = `${(e as Error).message ?? e}`;
          // The platform cannot allocate or fill the YUV layout (Android
          // before API 29).
          if (message.indexOf("unsupported") !== -1) {
            return { kind: "skip", reason: message };
          }
          return { kind: "fail", reason: message };
        }
        const plain = RNWebGPU.createNativeBufferFromPointer(source.handle);
        const wrapped = RNWebGPU.createNativeBufferFromPointer(source.handle, {
          colorSpace: override,
        });
        const release = () => {
          source.release();
          plain.release();
          wrapped.release();
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

          // Each draw fills a 2x1 target, whose pixel centers sample the
          // middle of the left (red) and right (green) halves of the frame,
          // and lands in its own 256-byte row of one readback buffer.
          const draws = [
            { source: plain },
            { source: wrapped },
            { source: plain, sourceColorSpace: override },
          ];
          const readback = device.createBuffer({
            size: 256 * draws.length,
            usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
          });
          const encoder = device.createCommandEncoder();
          const externalTextures = draws.map((descriptor, i) => {
            const externalTexture = device.importExternalTexture(descriptor);
            const target = device.createTexture({
              size: [2, 1],
              format: "rgba8unorm",
              usage:
                GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
            });
            const pass = encoder.beginRenderPass({
              colorAttachments: [
                {
                  view: target.createView(),
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
              { texture: target },
              { buffer: readback, offset: 256 * i, bytesPerRow: 256 },
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
              const data = new Uint8Array(readback.getMappedRange());
              const samples = (i: number): Samples => ({
                left: Array.from(data.subarray(256 * i, 256 * i + 4)),
                right: Array.from(data.subarray(256 * i + 4, 256 * i + 8)),
              });
              return {
                kind: "ok" as const,
                plain: samples(0),
                wrapped: samples(1),
                imported: samples(2),
              };
            })
            .finally(release);
        } catch (e) {
          release();
          return { kind: "fail", reason: `${(e as Error).message ?? e}` };
        }
      },
      { override: OVERRIDE },
    );

    if (result.kind === "skip") {
      console.log(`NativeBufferColorSpace: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`NativeBufferColorSpace: ${result.reason}`);
    }
    console.log(`NativeBufferColorSpace: ${JSON.stringify(result)}`);

    // `plain` is not checked: it is off only where the platform guesses
    // another color space than the frame's (BT.601 by default), and the log
    // shows it.
    expectColor(result.wrapped.left, [255, 0, 0, 255]);
    expectColor(result.wrapped.right, [0, 255, 0, 255]);
    expect(result.wrapped).toEqual(result.imported);
  });
});
