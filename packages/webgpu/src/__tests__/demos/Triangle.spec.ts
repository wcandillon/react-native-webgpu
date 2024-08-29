import { checkImage, client, encodeImage } from "../setup";

describe("Triangle", () => {
  it("Simple Triangle", async () => {
    const result = await client.eval(
      ({ device, shaders: { triangleVertWGSL, redFragWGSL }, gpu, ctx }) => {
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: device.createShaderModule({
              code: triangleVertWGSL,
            }),
          },
          fragment: {
            module: device.createShaderModule({
              code: redFragWGSL,
            }),
            targets: [
              {
                format: gpu.getPreferredCanvasFormat(),
              },
            ],
          },
          primitive: {
            topology: "triangle-list",
          },
        });

        const commandEncoder = device.createCommandEncoder();
        const textureView = ctx.getCurrentTexture().createView();

        const renderPassDescriptor: GPURenderPassDescriptor = {
          colorAttachments: [
            {
              view: textureView,
              clearValue: [0.3, 0.6, 1, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        };

        const passEncoder =
          commandEncoder.beginRenderPass(renderPassDescriptor);
        passEncoder.setPipeline(pipeline);
        passEncoder.draw(3);
        passEncoder.end();
        device.queue.submit([commandEncoder.finish()]);
        return ctx.getImageData();
      },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/triangle.png");
  });
  it("Async Simple Triangle", async () => {
    const result = await client.eval(
      ({ device, shaders: { triangleVertWGSL, redFragWGSL }, gpu, ctx }) => {
        return device
          .createRenderPipelineAsync({
            layout: "auto",
            vertex: {
              module: device.createShaderModule({
                code: triangleVertWGSL,
              }),
            },
            fragment: {
              module: device.createShaderModule({
                code: redFragWGSL,
              }),
              targets: [
                {
                  format: gpu.getPreferredCanvasFormat(),
                },
              ],
            },
            primitive: {
              topology: "triangle-list",
            },
          })
          .then((pipeline) => {
            const commandEncoder = device.createCommandEncoder();
            const textureView = ctx.getCurrentTexture().createView();

            const renderPassDescriptor: GPURenderPassDescriptor = {
              colorAttachments: [
                {
                  view: textureView,
                  clearValue: [0.3, 0.6, 1, 1],
                  loadOp: "clear",
                  storeOp: "store",
                },
              ],
            };

            const passEncoder =
              commandEncoder.beginRenderPass(renderPassDescriptor);
            passEncoder.setPipeline(pipeline);
            passEncoder.draw(3);
            passEncoder.end();
            device.queue.submit([commandEncoder.finish()]);
            return ctx.getImageData();
          });
      },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/triangle.png");
  });
  it("Triangle MSAA", async () => {
    const result = await client.eval(
      ({ device, shaders: { triangleVertWGSL, redFragWGSL }, gpu, ctx }) => {
        const sampleCount = 4;
        const presentationFormat = gpu.getPreferredCanvasFormat();
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: device.createShaderModule({
              code: triangleVertWGSL,
            }),
          },
          fragment: {
            module: device.createShaderModule({
              code: redFragWGSL,
            }),
            targets: [
              {
                format: presentationFormat,
              },
            ],
          },
          primitive: {
            topology: "triangle-list",
          },
          multisample: {
            count: sampleCount,
          },
        });

        const texture = device.createTexture({
          size: [ctx.width, ctx.height],
          sampleCount,
          format: presentationFormat,
          usage: GPUTextureUsage.RENDER_ATTACHMENT,
        });
        const view = texture.createView();

        const commandEncoder = device.createCommandEncoder();

        const renderPassDescriptor: GPURenderPassDescriptor = {
          colorAttachments: [
            {
              view,
              resolveTarget: ctx.getCurrentTexture().createView(),
              clearValue: [0, 0, 0, 1],
              loadOp: "clear",
              storeOp: "discard",
            },
          ],
        };

        const passEncoder =
          commandEncoder.beginRenderPass(renderPassDescriptor);
        passEncoder.setPipeline(pipeline);
        passEncoder.draw(3);
        passEncoder.end();
        device.queue.submit([commandEncoder.finish()]);
        return ctx.getImageData();
      },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/triangle-msaa.png");
  });
  it("Triangle with constants", async () => {
    const result = await client.eval(
      ({ device, shaders: { triangleVertWGSL }, gpu, ctx }) => {
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: device.createShaderModule({
              code: triangleVertWGSL,
            }),
          },
          fragment: {
            module: device.createShaderModule({
              code: `
      @vertex fn vs(
        @builtin(vertex_index) vertexIndex : u32
      ) -> @builtin(position) vec4f {
        let pos = array(
          vec2f( 0.0,  0.5),  // top center
          vec2f(-0.5, -0.5),  // bottom left
          vec2f( 0.5, -0.5)   // bottom right
        );

        return vec4f(pos[vertexIndex], 0.0, 1.0);
      }

      override red = 0.0;
      override green = 0.0;
      override blue = 0.0;

      @fragment fn fs() -> @location(0) vec4f {
        return vec4f(red, green, blue, 1.0);
      }
    `,
            }),
            targets: [
              {
                format: gpu.getPreferredCanvasFormat(),
              },
            ],
            constants: {
              red: 1,
              green: 0.5,
              blue: 1,
            },
          },
          primitive: {
            topology: "triangle-list",
          },
        });

        const commandEncoder = device.createCommandEncoder();
        const textureView = ctx.getCurrentTexture().createView();

        const renderPassDescriptor: GPURenderPassDescriptor = {
          colorAttachments: [
            {
              view: textureView,
              clearValue: [0.3, 0.6, 1, 1],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
        };

        const passEncoder =
          commandEncoder.beginRenderPass(renderPassDescriptor);
        passEncoder.setPipeline(pipeline);
        passEncoder.draw(3);
        passEncoder.end();
        device.queue.submit([commandEncoder.finish()]);
        return ctx.getImageData();
      },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/constant-triangle.png");
  });
});
