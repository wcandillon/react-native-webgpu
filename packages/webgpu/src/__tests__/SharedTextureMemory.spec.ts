import { checkImage, client, encodeImage } from "./setup";

interface BitmapData {
  data: number[];
  width: number;
  height: number;
  format: string;
}

type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | ({ kind: "ok" } & BitmapData);

describe("SharedTextureMemory", () => {
  it("imports a test frame and samples it through a textured quad", async () => {
    const result = await client.eval<Record<string, never>, EvalResult>(
      ({ device, gpu, ctx, canvas }) => {
        // The Dawn-specific feature is the *only* legitimate reason to skip:
        // the Chrome reference run, older Android adapters, or fallback
        // adapters won't have it. Anything else past this point is a real
        // failure and must surface as a test failure, not a silent skip.
        const candidates = [
          "shared-texture-memory-iosurface",
          "shared-texture-memory-ahardware-buffer",
        ];
        const supported = candidates.find((f) =>
          device.features.has(f as GPUFeatureName),
        );
        if (!supported) {
          return {
            kind: "skip",
            reason: "no shared-texture-memory feature on this adapter",
          };
        }
        if (typeof RNWebGPU?.createTestVideoFrame !== "function") {
          return {
            kind: "skip",
            reason: "RNWebGPU.createTestVideoFrame is unavailable",
          };
        }

        try {
          const frame = RNWebGPU.createTestVideoFrame(256, 256);
          const memory = device.importSharedTextureMemory({
            handle: frame.handle,
            label: "test-frame",
          });
          const texture = memory.createTexture();
          if (!memory.beginAccess(texture, true)) {
            frame.release();
            return {
              kind: "fail",
              reason: `beginAccess returned false (feature: ${supported})`,
            };
          }

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

              @group(0) @binding(0) var srcTex: texture_2d<f32>;
              @group(0) @binding(1) var srcSampler: sampler;

              @fragment fn fs(in: VsOut) -> @location(0) vec4f {
                return textureSample(srcTex, srcSampler, in.uv);
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
              { binding: 0, resource: texture.createView() },
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

          return canvas.getImageData().then((image: BitmapData) => {
            memory.endAccess(texture);
            texture.destroy();
            frame.release();
            return { kind: "ok" as const, ...image };
          });
        } catch (e) {
          return {
            kind: "fail",
            reason: `${(e as Error).message ?? e}`,
          };
        }
      },
    );

    if (result.kind === "skip") {
      console.log(`SharedTextureMemory: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`SharedTextureMemory: ${result.reason}`);
    }
    const image = encodeImage(result);
    checkImage(image, "snapshots/shared-texture-memory.png");
  });
});
