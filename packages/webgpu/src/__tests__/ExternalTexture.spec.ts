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

describe("External Textures", () => {
  it("Simple (1)", async () => {
    const result = await client.eval(
      ({ gpu, device, ctx, canvas, urls: { fTexture } }) => {
        const module = device.createShaderModule({
          label: "our hardcoded textured quad shaders",
          code: /* wgsl */ `
          struct OurVertexShaderOutput {
            @builtin(position) position: vec4f,
            @location(0) texcoord: vec2f,
          };
    
          @vertex fn vs(
            @builtin(vertex_index) vertexIndex : u32
          ) -> OurVertexShaderOutput {
            let pos = array(
              // 1st triangle
              vec2f( 0.0,  0.0),  // center
              vec2f( 1.0,  0.0),  // right, center
              vec2f( 0.0,  1.0),  // center, top
    
              // 2st triangle
              vec2f( 0.0,  1.0),  // center, top
              vec2f( 1.0,  0.0),  // right, center
              vec2f( 1.0,  1.0),  // right, top
            );
    
            var vsOutput: OurVertexShaderOutput;
            let xy = pos[vertexIndex];
            vsOutput.position = vec4f(xy, 0.0, 1.0);
            vsOutput.texcoord = xy;
            return vsOutput;
          }
    
          @group(0) @binding(0) var ourSampler: sampler;
          @group(0) @binding(1) var ourTexture: texture_2d<f32>;
    
          @fragment fn fs(fsInput: OurVertexShaderOutput) -> @location(0) vec4f {
            return textureSample(ourTexture, ourSampler, fsInput.texcoord);
          }
        `,
        });

        const presentationFormat = gpu.getPreferredCanvasFormat();
        const pipeline = device.createRenderPipeline({
          label: "hardcoded textured quad pipeline",
          layout: "auto",
          vertex: {
            module,
          },
          fragment: {
            module,
            targets: [{ format: presentationFormat }],
          },
        });

        return fetch(fTexture).then((res) => {
          return res.blob().then((blob) => {
            return createImageBitmap(blob, {
              colorSpaceConversion: "none",
            }).then((source) => {
              const texture = device.createTexture({
                label: fTexture,
                format: "rgba8unorm",
                size: [source.width, source.height],
                usage:
                  GPUTextureUsage.TEXTURE_BINDING |
                  GPUTextureUsage.COPY_DST |
                  GPUTextureUsage.RENDER_ATTACHMENT,
              });
              device.queue.copyExternalImageToTexture(
                { source, flipY: true },
                { texture },
                { width: source.width, height: source.height },
              );
              const bindGroups: GPUBindGroup[] = [];
              for (let i = 0; i < 8; ++i) {
                const sampler = device.createSampler({
                  addressModeU: i & 1 ? "repeat" : "clamp-to-edge",
                  addressModeV: i & 2 ? "repeat" : "clamp-to-edge",
                  magFilter: i & 4 ? "linear" : "nearest",
                });

                const bindGroup = device.createBindGroup({
                  layout: pipeline.getBindGroupLayout(0),
                  entries: [
                    { binding: 0, resource: sampler },
                    { binding: 1, resource: texture.createView() },
                  ],
                });
                bindGroups.push(bindGroup);
              }

              const renderPassDescriptor: GPURenderPassDescriptor = {
                label: "our basic canvas renderPass",
                colorAttachments: [
                  {
                    view: ctx.getCurrentTexture().createView(), // Assigned later
                    clearValue: [0.3, 0.3, 0.3, 1],
                    loadOp: "clear",
                    storeOp: "store",
                  },
                ],
              };

              const settings = {
                addressModeU: "repeat",
                addressModeV: "repeat",
                magFilter: "linear",
              };

              function render() {
                const ndx =
                  (settings.addressModeU === "repeat" ? 1 : 0) +
                  (settings.addressModeV === "repeat" ? 2 : 0) +
                  (settings.magFilter === "linear" ? 4 : 0);
                const bindGroup = bindGroups[ndx];

                const encoder = device.createCommandEncoder({
                  label: "render quad encoder",
                });
                const pass = encoder.beginRenderPass(renderPassDescriptor);
                pass.setPipeline(pipeline);
                pass.setBindGroup(0, bindGroup);
                pass.draw(6); // call our vertex shader 6 times
                pass.end();

                const commandBuffer = encoder.finish();
                device.queue.submit([commandBuffer]);
              }
              render();
              return canvas.getImageData();
            });
          });
        });
      },
      {},
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/f.png");
  });
  it("Simple (2)", async () => {
    const result = await client.eval(
      ({ gpu, device, ctx, canvas, urls: { fTexture } }) => {
        const module = device.createShaderModule({
          label: "our hardcoded textured quad shaders",
          code: /* wgsl */ `
          struct OurVertexShaderOutput {
            @builtin(position) position: vec4f,
            @location(0) texcoord: vec2f,
          };
    
          @vertex fn vs(
            @builtin(vertex_index) vertexIndex : u32
          ) -> OurVertexShaderOutput {
            let pos = array(
              // 1st triangle
              vec2f( 0.0,  0.0),  // center
              vec2f( 1.0,  0.0),  // right, center
              vec2f( 0.0,  1.0),  // center, top
    
              // 2st triangle
              vec2f( 0.0,  1.0),  // center, top
              vec2f( 1.0,  0.0),  // right, center
              vec2f( 1.0,  1.0),  // right, top
            );
    
            var vsOutput: OurVertexShaderOutput;
            let xy = pos[vertexIndex];
            vsOutput.position = vec4f(xy, 0.0, 1.0);
            vsOutput.texcoord = xy;
            return vsOutput;
          }
    
          @group(0) @binding(0) var ourSampler: sampler;
          @group(0) @binding(1) var ourTexture: texture_2d<f32>;
    
          @fragment fn fs(fsInput: OurVertexShaderOutput) -> @location(0) vec4f {
            return textureSample(ourTexture, ourSampler, fsInput.texcoord);
          }
        `,
        });

        const presentationFormat = gpu.getPreferredCanvasFormat();
        const pipeline = device.createRenderPipeline({
          label: "hardcoded textured quad pipeline",
          layout: "auto",
          vertex: {
            module,
          },
          fragment: {
            module,
            targets: [{ format: presentationFormat }],
          },
        });

        return fetch(fTexture).then((res) => {
          return res.blob().then((blob) => {
            return createImageBitmap(blob, {
              colorSpaceConversion: "none",
            }).then((source) => {
              const texture = device.createTexture({
                label: fTexture,
                format: "rgba8unorm",
                size: [source.width, source.height],
                usage:
                  GPUTextureUsage.TEXTURE_BINDING |
                  GPUTextureUsage.COPY_DST |
                  GPUTextureUsage.RENDER_ATTACHMENT,
              });
              device.queue.copyExternalImageToTexture(
                { source },
                { texture },
                { width: source.width, height: source.height },
              );
              const bindGroups: GPUBindGroup[] = [];
              for (let i = 0; i < 8; ++i) {
                const sampler = device.createSampler({
                  addressModeU: i & 1 ? "repeat" : "clamp-to-edge",
                  addressModeV: i & 2 ? "repeat" : "clamp-to-edge",
                  magFilter: i & 4 ? "linear" : "nearest",
                });

                const bindGroup = device.createBindGroup({
                  layout: pipeline.getBindGroupLayout(0),
                  entries: [
                    { binding: 0, resource: sampler },
                    { binding: 1, resource: texture.createView() },
                  ],
                });
                bindGroups.push(bindGroup);
              }

              const renderPassDescriptor: GPURenderPassDescriptor = {
                label: "our basic canvas renderPass",
                colorAttachments: [
                  {
                    view: ctx.getCurrentTexture().createView(), // Assigned later
                    clearValue: [0.3, 0.3, 0.3, 1],
                    loadOp: "clear",
                    storeOp: "store",
                  },
                ],
              };

              const settings = {
                addressModeU: "repeat",
                addressModeV: "repeat",
                magFilter: "linear",
              };

              function render() {
                const ndx =
                  (settings.addressModeU === "repeat" ? 1 : 0) +
                  (settings.addressModeV === "repeat" ? 2 : 0) +
                  (settings.magFilter === "linear" ? 4 : 0);
                const bindGroup = bindGroups[ndx];

                const encoder = device.createCommandEncoder({
                  label: "render quad encoder",
                });
                const pass = encoder.beginRenderPass(renderPassDescriptor);
                pass.setPipeline(pipeline);
                pass.setBindGroup(0, bindGroup);
                pass.draw(6); // call our vertex shader 6 times
                pass.end();

                const commandBuffer = encoder.finish();
                device.queue.submit([commandBuffer]);
              }
              render();
              return canvas.getImageData();
            });
          });
        });
      },
      {},
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/f2.png");
  });
  it("flipY: false should not flip (same as omitting flipY)", async () => {
    const result = await client.eval(
      ({ gpu, device, ctx, canvas, urls: { fTexture } }) => {
        const module = device.createShaderModule({
          label: "our hardcoded textured quad shaders",
          code: /* wgsl */ `
          struct OurVertexShaderOutput {
            @builtin(position) position: vec4f,
            @location(0) texcoord: vec2f,
          };

          @vertex fn vs(
            @builtin(vertex_index) vertexIndex : u32
          ) -> OurVertexShaderOutput {
            let pos = array(
              // 1st triangle
              vec2f( 0.0,  0.0),  // center
              vec2f( 1.0,  0.0),  // right, center
              vec2f( 0.0,  1.0),  // center, top

              // 2st triangle
              vec2f( 0.0,  1.0),  // center, top
              vec2f( 1.0,  0.0),  // right, center
              vec2f( 1.0,  1.0),  // right, top
            );

            var vsOutput: OurVertexShaderOutput;
            let xy = pos[vertexIndex];
            vsOutput.position = vec4f(xy, 0.0, 1.0);
            vsOutput.texcoord = xy;
            return vsOutput;
          }

          @group(0) @binding(0) var ourSampler: sampler;
          @group(0) @binding(1) var ourTexture: texture_2d<f32>;

          @fragment fn fs(fsInput: OurVertexShaderOutput) -> @location(0) vec4f {
            return textureSample(ourTexture, ourSampler, fsInput.texcoord);
          }
        `,
        });

        const presentationFormat = gpu.getPreferredCanvasFormat();
        const pipeline = device.createRenderPipeline({
          label: "hardcoded textured quad pipeline",
          layout: "auto",
          vertex: {
            module,
          },
          fragment: {
            module,
            targets: [{ format: presentationFormat }],
          },
        });

        return fetch(fTexture).then((res) => {
          return res.blob().then((blob) => {
            return createImageBitmap(blob, {
              colorSpaceConversion: "none",
            }).then((source) => {
              const texture = device.createTexture({
                label: fTexture,
                format: "rgba8unorm",
                size: [source.width, source.height],
                usage:
                  GPUTextureUsage.TEXTURE_BINDING |
                  GPUTextureUsage.COPY_DST |
                  GPUTextureUsage.RENDER_ATTACHMENT,
              });
              // Explicitly pass flipY: false - should behave same as omitting it
              device.queue.copyExternalImageToTexture(
                { source, flipY: false },
                { texture },
                { width: source.width, height: source.height },
              );
              const sampler = device.createSampler({
                addressModeU: "repeat",
                addressModeV: "repeat",
                magFilter: "linear",
              });

              const bindGroup = device.createBindGroup({
                layout: pipeline.getBindGroupLayout(0),
                entries: [
                  { binding: 0, resource: sampler },
                  { binding: 1, resource: texture.createView() },
                ],
              });

              const renderPassDescriptor: GPURenderPassDescriptor = {
                label: "our basic canvas renderPass",
                colorAttachments: [
                  {
                    view: ctx.getCurrentTexture().createView(),
                    clearValue: [0.3, 0.3, 0.3, 1],
                    loadOp: "clear",
                    storeOp: "store",
                  },
                ],
              };

              const encoder = device.createCommandEncoder({
                label: "render quad encoder",
              });
              const pass = encoder.beginRenderPass(renderPassDescriptor);
              pass.setPipeline(pipeline);
              pass.setBindGroup(0, bindGroup);
              pass.draw(6);
              pass.end();

              const commandBuffer = encoder.finish();
              device.queue.submit([commandBuffer]);
              return canvas.getImageData();
            });
          });
        });
      },
      {},
    );
    const image = encodeImage(result);
    // flipY: false should produce the same result as omitting flipY (f2.png)
    // This test catches the bug where std::optional<bool> was checked incorrectly
    checkImage(image, "snapshots/f2.png");
  });

  // Regression test for createBindGroupLayout with a `texture_external` entry.
  // Every other path uses layout: "auto", which never builds an explicit
  // BindGroupLayoutEntry for an external texture, so the native conversion that
  // chains ExternalTextureBindingLayout went untested. Here we build the layout
  // ourselves (externalTexture: {}) and render a GPUExternalTexture through it.
  // Device-only: needs a native frame, so it skips on the web reference.
  it("samples a GPUExternalTexture through an explicit bind group layout", async () => {
    const result = await client.eval<Record<string, never>, EvalResult>(
      ({ device, gpu, ctx, canvas }) => {
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
          const externalTexture = device.importExternalTexture({
            // createTestVideoFrame returns our NativeVideoFrame; the native
            // binding accepts it, but the spec type wants a WebCodecs
            // VideoFrame, so cast to satisfy the signature.
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
          // The whole point of the test: an explicit external-texture layout.
          const bindGroupLayout = device.createBindGroupLayout({
            entries: [
              {
                binding: 0,
                visibility: GPUShaderStage.FRAGMENT,
                externalTexture: {},
              },
              { binding: 1, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
            ],
          });
          const pipeline = device.createRenderPipeline({
            layout: device.createPipelineLayout({
              bindGroupLayouts: [bindGroupLayout],
            }),
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
            layout: bindGroupLayout,
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
      console.log(
        `ExternalTexture (explicit layout): skipping (${result.reason})`,
      );
      return;
    }
    if (result.kind === "fail") {
      throw new Error(`ExternalTexture (explicit layout): ${result.reason}`);
    }
    const image = encodeImage(result);
    // Same render as the auto-layout path in ImportExternalTexture.spec.ts, so
    // it must match that snapshot bit-for-bit.
    checkImage(image, "snapshots/import-external-texture.png");
  });
});
