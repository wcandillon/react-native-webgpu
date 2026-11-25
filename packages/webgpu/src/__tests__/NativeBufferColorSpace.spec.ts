import { client } from "./setup";

// A pixel as [r, g, b, a], 0..255.
type Rgba = number[];

type Samples = { left: Rgba; right: Rgba };

type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | {
      kind: "ok";
      // The pointer wrapped without options, imported as is.
      plain: Samples;
      // The pointer wrapped with the colorSpace option, imported as is.
      wrapped: Samples;
      // The pointer wrapped without options, imported with the same color
      // space as sourceColorSpace.
      imported: Samples;
    };

// The override: neither what the test frame reports (BT.709 limited range)
// nor the fallback for a buffer that reports nothing (BT.601 limited range),
// so that it changes the result whatever the buffer says.
const OVERRIDE = { matrix: "rec2020", range: "full" };

describe("createNativeBufferFromPointer colorSpace", () => {
  // The option sets the color space of the wrapped buffer: importing it must
  // give what importExternalTexture's sourceColorSpace gives for the same
  // members, and not what the buffer says on its own. It is compared with
  // sourceColorSpace rather than with computed colors, so that it holds
  // whatever conversion the platform does.
  //
  // Android only: the pointer comes from the handle of the NV12 test frame,
  // an AHardwareBuffer*, which createNativeBufferFromPointer takes. On Apple
  // platforms the handle is an IOSurfaceRef, while createNativeBufferFromPointer
  // takes a CVPixelBufferRef, which nothing in JS gives.
  it("reads the buffer with the color space it is given", async () => {
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
        // Android samples YUV buffers through Dawn's opaque YCbCr path, which
        // the device only has when the adapter supports Vulkan YCbCr sampling.
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

        type Frame = ReturnType<typeof RNWebGPU.createTestVideoFrame>;
        // Typed loosely so that the spec also builds against a version
        // without the option, where it fails rather than not compiling.
        const native = RNWebGPU as unknown as {
          createNativeBufferFromPointer(
            pointer: bigint,
            options?: unknown,
          ): Frame;
        };
        const wrap = (pointer: bigint, options?: unknown) =>
          native.createNativeBufferFromPointer(pointer, options);

        let source: Frame;
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

        const frames: Frame[] = [source];
        const release = () => frames.forEach((frame) => frame.release());

        try {
          const plainFrame = wrap(source.handle);
          frames.push(plainFrame);
          const wrappedFrame = wrap(source.handle, { colorSpace: override });
          frames.push(wrappedFrame);

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

          const SIZE = 64;
          const BYTES_PER_ROW = SIZE * 4;

          // Samples `frame` onto a 64x64 RGBA texture and reads it back.
          const draw = (
            frame: Frame,
            sourceColorSpace?: typeof override,
          ): Promise<Samples> => {
            const externalTexture = device.importExternalTexture({
              source: frame,
              ...(sourceColorSpace ? { sourceColorSpace } : {}),
            } as unknown as GPUExternalTextureDescriptor);
            const target = device.createTexture({
              size: [SIZE, SIZE],
              format: "rgba8unorm",
              usage:
                GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
            });
            const readback = device.createBuffer({
              size: BYTES_PER_ROW * SIZE,
              usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
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
                  view: target.createView(),
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
            encoder.copyTextureToBuffer(
              { texture: target },
              { buffer: readback, bytesPerRow: BYTES_PER_ROW },
              [SIZE, SIZE],
            );
            device.queue.submit([encoder.finish()]);
            externalTexture.destroy();

            return readback.mapAsync(GPUMapMode.READ).then(() => {
              const data = new Uint8Array(readback.getMappedRange());
              const pixel = (x: number, y: number): Rgba => {
                const i = y * BYTES_PER_ROW + x * 4;
                return [data[i], data[i + 1], data[i + 2], data[i + 3]];
              };
              const midY = SIZE / 2;
              const samples = {
                left: pixel(SIZE / 4, midY),
                right: pixel((SIZE * 3) / 4, midY),
              };
              readback.unmap();
              readback.destroy();
              target.destroy();
              return samples;
            });
          };

          return draw(plainFrame)
            .then((plain) =>
              draw(wrappedFrame).then((wrapped) =>
                draw(plainFrame, override).then((imported) => ({
                  kind: "ok" as const,
                  plain,
                  wrapped,
                  imported,
                })),
              ),
            )
            .then(
              (samples) => {
                release();
                return samples;
              },
              (e: unknown) => {
                release();
                return {
                  kind: "fail" as const,
                  reason: `${(e as Error).message ?? e}`,
                };
              },
            );
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
    console.log(
      `NativeBufferColorSpace: ${JSON.stringify({
        plain: result.plain,
        wrapped: result.wrapped,
        imported: result.imported,
      })}`,
    );

    // The same conversion as sourceColorSpace, to the level: both resolve
    // the same color space for the same buffer.
    expect(result.wrapped).toEqual(result.imported);
    // And not the buffer's own: the override really took effect. Full range
    // BT.2020 is far from limited range BT.709 / BT.601 for saturated red and
    // green, so the largest channel difference is well above rounding.
    const distance = (a: Rgba, b: Rgba) =>
      Math.max(...a.map((channel, i) => Math.abs(channel - b[i])));
    expect(distance(result.wrapped.left, result.plain.left)).toBeGreaterThan(
      10,
    );
    expect(distance(result.wrapped.right, result.plain.right)).toBeGreaterThan(
      10,
    );
  });
});
