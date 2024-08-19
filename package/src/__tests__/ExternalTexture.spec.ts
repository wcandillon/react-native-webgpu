import { checkImage, client, encodeImage } from "./setup";

const imageURL =
  // eslint-disable-next-line max-len
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAQAAAAEACAIAAADTED8xAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAyBpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADw/eHBhY2tldCBiZWdpbj0i77u/IiBpZD0iVzVNME1wQ2VoaUh6cmVTek5UY3prYzlkIj8+IDx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IkFkb2JlIFhNUCBDb3JlIDUuMC1jMDYwIDYxLjEzNDc3NywgMjAxMC8wMi8xMi0xNzozMjowMCAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvIiB4bWxuczp4bXBNTT0iaHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wL21tLyIgeG1sbnM6c3RSZWY9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9zVHlwZS9SZXNvdXJjZVJlZiMiIHhtcDpDcmVhdG9yVG9vbD0iQWRvYmUgUGhvdG9zaG9wIENTNSBXaW5kb3dzIiB4bXBNTTpJbnN0YW5jZUlEPSJ4bXAuaWlkOjBGNUE1QUEzOTYxQTExREY4QkFDQTUxM0JENDg1MTlDIiB4bXBNTTpEb2N1bWVudElEPSJ4bXAuZGlkOjBGNUE1QUE0OTYxQTExREY4QkFDQTUxM0JENDg1MTlDIj4gPHhtcE1NOkRlcml2ZWRGcm9tIHN0UmVmOmluc3RhbmNlSUQ9InhtcC5paWQ6MEY1QTVBQTE5NjFBMTFERjhCQUNBNTEzQkQ0ODUxOUMiIHN0UmVmOmRvY3VtZW50SUQ9InhtcC5kaWQ6MEY1QTVBQTI5NjFBMTFERjhCQUNBNTEzQkQ0ODUxOUMiLz4gPC9yZGY6RGVzY3JpcHRpb24+IDwvcmRmOlJERj4gPC94OnhtcG1ldGE+IDw/eHBhY2tldCBlbmQ9InIiPz5ernmvAAAxeklEQVR42ux9z68kyXFe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";

describe("External Textures", () => {
  it("Simple (1)", async () => {
    const result = await client.eval(
      ({ gpu, device, ctx, url }) => {
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

        return fetch(url).then((res) => {
          return res.blob().then((blob) => {
            return createImageBitmap(blob, {
              colorSpaceConversion: "none",
            }).then((source) => {
              const texture = device.createTexture({
                label: url,
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
              return ctx.getImageData();
            });
          });
        });
      },
      { url: imageURL },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/f.png");
  });
  it("Simple (2)", async () => {
    const result = await client.eval(
      ({ gpu, device, ctx, url }) => {
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

        return fetch(url).then((res) => {
          return res.blob().then((blob) => {
            return createImageBitmap(blob, {
              colorSpaceConversion: "none",
            }).then((source) => {
              const texture = device.createTexture({
                label: url,
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
              return ctx.getImageData();
            });
          });
        });
      },
      { url: imageURL },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/f2.png");
  });
});
