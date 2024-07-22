import { checkImage, client, encodeImage } from "../setup";

describe("Cube", () => {
  it("Rotating cube", async () => {
    const result = await client.eval(
      ({ gpu, device, assets: { cubeVertexArray }, ctx, mat4, vec3 }) => {
        const cubeVertexSize = 4 * 10; // Byte size of one cube vertex.
        const cubePositionOffset = 0;
        const cubeUVOffset = 4 * 8;
        const cubeVertexCount = 36;
        const basicVertWGSL = /*wgsl*/ `struct Uniforms {
          modelViewProjectionMatrix : mat4x4f,
        }
        @binding(0) @group(0) var<uniform> uniforms : Uniforms;
        
        struct VertexOutput {
          @builtin(position) Position : vec4f,
          @location(0) fragUV : vec2f,
          @location(1) fragPosition: vec4f,
        }
        
        @vertex
        fn main(
          @location(0) position : vec4f,
          @location(1) uv : vec2f
        ) -> VertexOutput {
          var output : VertexOutput;
          output.Position = uniforms.modelViewProjectionMatrix * position;
          output.fragUV = uv;
          output.fragPosition = 0.5 * (position + vec4(1.0, 1.0, 1.0, 1.0));
          return output;
        }
        `;
        const vertexPositionColorWGSL = /*wgsl*/ `@fragment
        fn main(
          @location(0) fragUV: vec2f,
          @location(1) fragPosition: vec4f
        ) -> @location(0) vec4f {
          return fragPosition;
        }`;
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
              code: vertexPositionColorWGSL,
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

        const uniformBindGroup = device.createBindGroup({
          layout: pipeline.getBindGroupLayout(0),
          entries: [
            {
              binding: 0,
              resource: {
                buffer: uniformBuffer,
              },
            },
          ],
        });

        const renderPassDescriptor: GPURenderPassDescriptor = {
          colorAttachments: [
            {
              view: ctx.getCurrentTexture().createView(), // Assigned later

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

        function getTransformationMatrix() {
          const viewMatrix = mat4.identity();
          mat4.translate(viewMatrix, vec3.fromValues(0, 0, -4), viewMatrix);
          const now = 1721677648;
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

        const transformationMatrix = getTransformationMatrix();
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
        device.queue.submit([commandEncoder.finish()]);
        return ctx.getImageData();
      },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/cube.png");
  });
});
