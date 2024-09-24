import { checkImage, client, encodeImage } from "./setup";

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
});
