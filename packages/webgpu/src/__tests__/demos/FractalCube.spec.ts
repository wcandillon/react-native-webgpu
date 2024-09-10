import { checkImage, client, encodeImage } from "../setup";

import { basicVert } from "./Cube.spec";

const sampleSelf = /*wgsl*/ `@binding(1) @group(0) var mySampler: sampler;
@binding(2) @group(0) var myTexture: texture_2d<f32>;

@fragment
fn main(
  @location(0) fragUV: vec2f,
  @location(1) fragPosition: vec4f
) -> @location(0) vec4f {
  let texColor = textureSample(myTexture, mySampler, fragUV * 0.8 + vec2(0.1));
  let f = select(1.0, 0.0, length(texColor.rgb - vec3(0.5)) < 0.01);
  return f * texColor + (1.0 - f) * fragPosition;
}
`;

describe("Fractal Cube", () => {
  it("Fractal Cube", async () => {
    const result = await client.eval(
      ({
        gpu,
        device,
        assets: { cubeVertexArray },
        ctx,
        mat4,
        vec3,
        sampleSelfWGSL,
        basicVertWGSL,
      }) => {
        const cubeVertexSize = 4 * 10; // Byte size of one cube vertex.
        const cubePositionOffset = 0;
        const cubeUVOffset = 4 * 8;
        const cubeVertexCount = 36;
        const presentationFormat = gpu.getPreferredCanvasFormat();
        // Create a vertex buffer from the cube data.
        const verticesBuffer = device.createBuffer({
          size: cubeVertexArray.byteLength,
          usage: GPUBufferUsage.VERTEX,
          mappedAtCreation: true,
        });
        new Float32Array(verticesBuffer.getMappedRange()).set(cubeVertexArray);
        verticesBuffer.unmap();

        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: device.createShaderModule({
              code: basicVertWGSL,
            }),
            buffers: [
              {
                arrayStride: cubeVertexSize,
                attributes: [
                  {
                    // position
                    shaderLocation: 0,
                    offset: cubePositionOffset,
                    format: "float32x4",
                  },
                  {
                    // uv
                    shaderLocation: 1,
                    offset: cubeUVOffset,
                    format: "float32x2",
                  },
                ],
              },
            ],
          },
          fragment: {
            module: device.createShaderModule({
              code: sampleSelfWGSL,
            }),
            targets: [
              {
                format: presentationFormat,
              },
            ],
          },
          primitive: {
            topology: "triangle-list",

            // Backface culling since the cube is solid piece of geometry.
            // Faces pointing away from the camera will be occluded by faces
            // pointing toward the camera.
            cullMode: "back",
          },

          // Enable depth testing so that the fragment closest to the camera
          // is rendered in front.
          depthStencil: {
            depthWriteEnabled: true,
            depthCompare: "less",
            format: "depth24plus",
          },
        });

        const depthTexture = device.createTexture({
          size: [ctx.width, ctx.height],
          format: "depth24plus",
          usage: GPUTextureUsage.RENDER_ATTACHMENT,
        });

        const uniformBufferSize = 4 * 16; // 4x4 matrix
        const uniformBuffer = device.createBuffer({
          size: uniformBufferSize,
          usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
        });

        // We will copy the frame's rendering results into this texture and
        // sample it on the next frame.
        const cubeTexture = device.createTexture({
          size: [ctx.width, ctx.height],
          format: presentationFormat,
          usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
        });

        // Create a sampler with linear filtering for smooth interpolation.
        const sampler = device.createSampler({
          magFilter: "linear",
          minFilter: "linear",
        });

        const uniformBindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [
            {
              binding: 0,
              resource: {
                buffer: uniformBuffer,
              },
            },
            {
              binding: 1,
              resource: sampler,
            },
            {
              binding: 2,
              resource: cubeTexture.createView(),
            },
          ],
        });

        const swapChainTexture = ctx.getCurrentTexture();
        const renderPassDescriptor: GPURenderPassDescriptor = {
          colorAttachments: [
            {
              view: swapChainTexture.createView(), // Assigned later

              clearValue: [0.5, 0.5, 0.5, 1.0],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
          depthStencilAttachment: {
            view: depthTexture.createView(),

            depthClearValue: 1.0,
            depthLoadOp: "clear",
            depthStoreOp: "store",
          },
        };

        const aspect = ctx.width / ctx.height;
        const projectionMatrix = mat4.perspective(
          (2 * Math.PI) / 5,
          aspect,
          1,
          100.0,
        );
        const modelViewProjectionMatrix = mat4.create();

        function getTransformationMatrix(now: number) {
          const viewMatrix = mat4.identity();
          mat4.translate(viewMatrix, vec3.fromValues(0, 0, -4), viewMatrix);
          mat4.rotate(
            viewMatrix,
            vec3.fromValues(Math.sin(now), Math.cos(now), 0),
            1,
            viewMatrix,
          );

          mat4.multiply(
            projectionMatrix,
            viewMatrix,
            modelViewProjectionMatrix,
          );

          return modelViewProjectionMatrix;
        }
        function frame(now: number) {
          const transformationMatrix = getTransformationMatrix(now);
          device.queue.writeBuffer(
            uniformBuffer,
            0,
            transformationMatrix.buffer,
            transformationMatrix.byteOffset,
            transformationMatrix.byteLength,
          );

          const commandEncoder = device.createCommandEncoder();
          const passEncoder =
            commandEncoder.beginRenderPass(renderPassDescriptor);
          passEncoder.setPipeline(pipeline);
          passEncoder.setBindGroup(0, uniformBindGroup);
          passEncoder.setVertexBuffer(0, verticesBuffer);
          passEncoder.draw(cubeVertexCount);
          passEncoder.end();

          // Copy the rendering results from the swapchain into |cubeTexture|.
          commandEncoder.copyTextureToTexture(
            {
              texture: swapChainTexture,
            },
            {
              texture: cubeTexture,
            },
            [ctx.width, ctx.height],
          );

          device.queue.submit([commandEncoder.finish()]);
        }
        const now = 1721677648;
        for (let i = 0; i < 10; i++) {
          frame(now + i * 16);
        }
        return ctx.getImageData();
      },
      {
        basicVertWGSL: basicVert,
        sampleSelfWGSL: sampleSelf,
      },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/fractal-cubes.png");
  });
});
