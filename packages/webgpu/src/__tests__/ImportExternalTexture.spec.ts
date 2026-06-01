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

describe("ImportExternalTexture", () => {
  it("imports a test frame as an external texture and samples it", async () => {
    const result = await client.eval<Record<string, never>, EvalResult>(
      ({ device, gpu, ctx, canvas }) => {
        // The umbrella feature backs importExternalTexture's IOSurface /
        // AHardwareBuffer import. The Chrome reference run and fallback adapters
        // won't advertise it, so that's the *only* legitimate skip. Anything
        // past this point is a real failure.
        const FEATURE = "rnwebgpu/native-texture";
        if (!device.features.has(FEATURE as GPUFeatureName)) {
          return {
            kind: "skip",
            reason: `${FEATURE} not enabled on this device`,
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
          // Unlike importSharedTextureMemory, there is no createTexture /
          // beginAccess / endAccess to manage: the GPUExternalTexture owns the
          // shared-memory access window and keeps the frame alive internally.
          const externalTexture = device.importExternalTexture({
            // createTestVideoFrame returns our NativeVideoFrame; the native
            // importExternalTexture binding accepts it, but the spec type wants
            // a WebCodecs VideoFrame, so cast to satisfy the signature.
            source: frame as unknown as VideoFrame,
            label: "test-frame",
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

          return canvas.getImageData().then((image: BitmapData) => {
            // Safe to release now: all GPU work referencing the frame is
            // submitted and the pixels have been read back.
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
      console.log(`ImportExternalTexture: skipping (${result.reason})`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`ImportExternalTexture: ${result.reason}`);
    }
    const image = encodeImage(result);
    checkImage(image, "snapshots/import-external-texture.png");
  });
});
