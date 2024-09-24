import { checkImage, client, encodeImage } from "./setup";

describe("Texture", () => {
  it("Check usage", async () => {
    const result = await client.eval(() => {
      return (
        GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING
      );
    });
    expect(result).toBe(20);
  });
  it("create texture (1)", async () => {
    const result = await client.eval(({ device }) => {
      const textureSize = 512;
      const texture = device.createTexture({
        size: [textureSize, textureSize],
        format: "rgba8unorm",
        usage:
          GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
        label: "texture",
      });
      return [
        texture.label,
        texture.width,
        texture.height,
        texture.depthOrArrayLayers,
        texture.mipLevelCount,
        texture.sampleCount,
        texture.dimension,
        texture.format,
        texture.usage,
      ];
    });
    expect(result).toEqual([
      "texture",
      512,
      512,
      1,
      1,
      1,
      "2d",
      "rgba8unorm",
      20,
    ]);
  });
  it("Write to texture", async () => {
    const result = await client.eval(
      ({ device, shaders: { triangleVertWGSL, redFragWGSL } }) => {
        // Define the size of the off-screen texture
        const textureWidth = 800;
        const textureHeight = 600;

        // Calculate bytesPerRow to be a multiple of 256
        const bytesPerRow = Math.ceil((textureWidth * 4) / 256) * 256;

        // Calculate the required buffer size
        const bufferSize = bytesPerRow * textureHeight;

        // Create the off-screen texture
        const texture = device.createTexture({
          size: [textureWidth, textureHeight],
          format: "rgba8unorm",
          usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
        });

        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: device.createShaderModule({
              code: triangleVertWGSL,
            }),
            entryPoint: "main",
          },
          fragment: {
            module: device.createShaderModule({
              code: redFragWGSL,
            }),
            entryPoint: "main",
            targets: [
              {
                format: "rgba8unorm",
              },
            ],
          },
          primitive: {
            topology: "triangle-list",
          },
        });

        // Create a buffer to store the rendered image
        const outputBuffer = device.createBuffer({
          size: bufferSize,
          usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
        });

        const commandEncoder = device.createCommandEncoder();
        const textureView = texture.createView();

        const renderPassDescriptor: GPURenderPassDescriptor = {
          colorAttachments: [
            {
              view: textureView,
              clearValue: [0, 0, 0, 1],
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

        // Copy the rendered texture to the output buffer
        commandEncoder.copyTextureToBuffer(
          { texture },
          { buffer: outputBuffer, bytesPerRow: bytesPerRow },
          [textureWidth, textureHeight],
        );

        device.queue.submit([commandEncoder.finish()]);
        return [
          texture.label,
          texture.width,
          texture.height,
          texture.depthOrArrayLayers,
          texture.mipLevelCount,
          texture.sampleCount,
          texture.dimension,
          texture.format,
          texture.usage,
        ];
      },
    );
    expect(result).toEqual(["", 800, 600, 1, 1, 1, "2d", "rgba8unorm", 17]);
  });
  it("Create texture and reads it", async () => {
    const result = await client.eval(
      ({
        device,
        shaders: { triangleVertWGSL, redFragWGSL },
        gpu,
        ctx,
        canvas,
      }) => {
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
              clearValue: [0, 0, 0, 0],
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
        return canvas.getImageData();
      },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/texture.png");
  });
});
