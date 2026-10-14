import { client } from "./setup";

// A pixel as [r, g, b], 0..255.
type Rgb = number[];

type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | { kind: "ok"; left: Rgb; right: Rgb };

const expectColor = (actual: Rgb, expected: Rgb, tolerance: number) => {
  expected.forEach((channel, i) => {
    expect(Math.abs(actual[i] - channel)).toBeLessThanOrEqual(tolerance);
  });
};

describe("GPUExternalTexture color space", () => {
  // The YUV test frame is BT.709 limited range, red on the left half and
  // green on the right. Converting it with the color space it declares must
  // give those colors back: the external texture does the conversion while
  // sampling, so a plain textureSampleBaseClampToEdge is enough, on both
  // platforms.
  it("converts a YUV frame to RGB with the color space of the frame", async () => {
    const result = await client.eval<Record<string, never>, EvalResult>(
      ({ device, gpu, ctx, canvas }) => {
        const NATIVE_IMPORT = [
          "shared-texture-memory-iosurface",
          "shared-texture-memory-ahardware-buffer",
        ];
        if (
          !NATIVE_IMPORT.some((f) => device.features.has(f as GPUFeatureName))
        ) {
          return {
            kind: "skip",
            reason: "native frame import is unavailable on this device",
          };
        }
        if (typeof RNWebGPU?.createTestVideoFrame !== "function") {
          return {
            kind: "skip",
            reason: "RNWebGPU.createTestVideoFrame is unavailable",
          };
        }
        // Android samples YUV buffers through Dawn's opaque YCbCr path, which
        // the device only has when the adapter supports Vulkan YCbCr sampling.
        if (
          device.features.has(
            "shared-texture-memory-ahardware-buffer" as GPUFeatureName,
          ) &&
          !device.features.has(
            "opaque-ycbcr-android-for-external-texture" as GPUFeatureName,
          )
        ) {
          return {
            kind: "skip",
            reason: "the device cannot sample YUV hardware buffers",
          };
        }

        let frame: ReturnType<typeof RNWebGPU.createTestVideoFrame>;
        try {
          frame = RNWebGPU.createTestVideoFrame(64, 64, "nv12");
        } catch (e) {
          const message = `${(e as Error).message ?? e}`;
          // The platform cannot allocate or fill the YUV layout (Android
          // before API 29).
          if (message.indexOf("unsupported") !== -1) {
            return { kind: "skip", reason: message };
          }
          return { kind: "fail", reason: message };
        }

        try {
          const externalTexture = device.importExternalTexture({
            source: frame,
            label: "yuv-test-frame",
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
          return {
            kind: "fail",
            reason: `${(e as Error).message ?? e}`,
          };
        }
      },
    );

    if (result.kind === "skip") {
      console.log(`ExternalTextureColorSpace: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`ExternalTextureColorSpace: ${result.reason}`);
    }
    // 8-bit YCbCr quantization and the transfer function round trip leave a
    // few levels of slack.
    expectColor(result.left, [255, 0, 0], 8);
    expectColor(result.right, [0, 255, 0], 8);
  });
});
