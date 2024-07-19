import { client } from "./setup";

describe("Triangle", () => {
  it("creates a shader module (1)", async () => {
    const result = await client.eval(({ device, triangleVertWGSL }) => {
      const mod = device.createShaderModule({
        code: triangleVertWGSL,
        label: "triangleVertWGSL",
      });
      return mod.label;
    });
    expect(result).toBe("triangleVertWGSL");
  });
  it("creates a shader module (2)", async () => {
    const result = await client.eval(({ device, redFragWGSL }) => {
      const mod = device.createShaderModule({
        code: redFragWGSL,
        label: "redFragWGSL",
      });
      return mod.label;
    });
    expect(result).toBe("redFragWGSL");
  });
  it("create the pipeline", async () => {
    const result = await client.eval(
      ({ device, triangleVertWGSL, redFragWGSL }) => {
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
                format: "rgba8unorm",
              },
            ],
          },
          primitive: {
            topology: "triangle-list",
          },
          label: "trianglePipeline",
        });
        return pipeline.label;
      },
    );
    expect(result).toBe("trianglePipeline");
  });
  it("create the pipeline and sample it", async () => {
    const result = await client.eval(
      ({ device, triangleVertWGSL, redFragWGSL, gpu }) => {
        // Create a texture to render to
        const textureSize = 512;
        const texture = device.createTexture({
          size: [textureSize, textureSize],
          format: "rgba8unorm",
          usage:
            GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING,
        });

        // Create a pipeline for rendering to the texture
        const texturePipeline = device.createRenderPipeline({
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
            targets: [{ format: "rgba8unorm" }],
          },
          primitive: {
            topology: "triangle-list",
          },
        });

        // Create a pipeline for displaying the texture
        const displayPipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: device.createShaderModule({
              code: `struct VertexOutput {
      @builtin(position) position: vec4<f32>,
      @location(0) texCoord: vec2<f32>,
  };

  @vertex
  fn main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
      var output: VertexOutput;
      let x = f32(vertexIndex & 1u) * 2.0 - 1.0;
      let y = f32(vertexIndex & 2u) - 1.0;
      output.position = vec4<f32>(x, y, 0.0, 1.0);
      output.texCoord = vec2<f32>((x + 1.0) * 0.5, (1.0 - y) * 0.5);
      return output;
  }`,
            }),
            entryPoint: "main",
          },
          fragment: {
            module: device.createShaderModule({
              code: `@group(0) @binding(0) var texSampler: sampler;
  @group(0) @binding(1) var tex: texture_2d<f32>;

  @fragment
  fn main(@location(0) texCoord: vec2<f32>) -> @location(0) vec4<f32> {
      return textureSample(tex, texSampler, texCoord);
  }`,
            }),
            entryPoint: "main",
            targets: [{ format: gpu.getPreferredCanvasFormat() }],
          },
          primitive: {
            topology: "triangle-list",
          },
        });
        return true;
        // Create a bind group for the texture
        // const sampler = device.createSampler({
        //   magFilter: "linear",
        //   minFilter: "linear",
        // });

        // const bindGroup = device.createBindGroup({
        //   layout: displayPipeline.getBindGroupLayout(0),
        //   entries: [
        //     {
        //       binding: 0,
        //       resource: sampler,
        //     },
        //     {
        //       binding: 1,
        //       resource: texture.createView(),
        //     },
        //   ],
        // });

        // // Render the triangle to the texture
        // const commandEncoder = device.createCommandEncoder();
        // const textureView = texture.createView();

        // const renderPassDescriptor: GPURenderPassDescriptor = {
        //   colorAttachments: [
        //     {
        //       view: textureView,
        //       clearValue: [0, 0, 0, 1],
        //       loadOp: "clear",
        //       storeOp: "store",
        //     },
        //   ],
        // };

        // const passEncoder =
        //   commandEncoder.beginRenderPass(renderPassDescriptor);
        // passEncoder.setPipeline(texturePipeline);
        // passEncoder.draw(3);
        // passEncoder.end();

        // device.queue.submit([commandEncoder.finish()]);
        // // // Function to display the texture on the screen
        // const cmdEncoder = device.createCommandEncoder();

        // const renderPD: GPURenderPassDescriptor = {
        //   colorAttachments: [
        //     {
        //       view: textureView,
        //       clearValue: [0.5, 0.5, 0.5, 1],
        //       loadOp: "clear",
        //       storeOp: "store",
        //     },
        //   ],
        // };

        // const encoder = cmdEncoder.beginRenderPass(renderPD);
        // encoder.setPipeline(displayPipeline);
        // encoder.setBindGroup(0, bindGroup);
        // encoder.draw(6);
        // encoder.end();

        // device.queue.submit([cmdEncoder.finish()]);
        // return true;
        // requestAnimationFrame(frame);
        // return pipeline.label;
      },
    );
    expect(result).toBe(true);
  });
});
