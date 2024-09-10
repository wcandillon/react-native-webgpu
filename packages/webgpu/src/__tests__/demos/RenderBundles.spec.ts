/* eslint-disable @typescript-eslint/ban-ts-comment */
import seedrandom from "seedrandom";

import { checkImage, client, encodeImage } from "../setup";

const mesh = /*wgsl*/ `struct Uniforms {
  viewProjectionMatrix : mat4x4f
}
@group(0) @binding(0) var<uniform> uniforms : Uniforms;

@group(1) @binding(0) var<uniform> modelMatrix : mat4x4f;

struct VertexInput {
  @location(0) position : vec4f,
  @location(1) normal : vec3f,
  @location(2) uv : vec2f
}

struct VertexOutput {
  @builtin(position) position : vec4f,
  @location(0) normal: vec3f,
  @location(1) uv : vec2f,
}

@vertex
fn vertexMain(input: VertexInput) -> VertexOutput {
  var output : VertexOutput;
  output.position = uniforms.viewProjectionMatrix * modelMatrix * input.position;
  output.normal = normalize((modelMatrix * vec4(input.normal, 0)).xyz);
  output.uv = input.uv;
  return output;
}

@group(1) @binding(1) var meshSampler: sampler;
@group(1) @binding(2) var meshTexture: texture_2d<f32>;

// Static directional lighting
const lightDir = vec3f(1, 1, 1);
const dirColor = vec3(1);
const ambientColor = vec3f(0.05);

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4f {
  let textureColor = textureSample(meshTexture, meshSampler, input.uv);

  // Very simplified lighting algorithm.
  let lightColor = saturate(ambientColor + max(dot(input.normal, lightDir), 0.0) * dirColor);

  return vec4f(textureColor.rgb * lightColor, textureColor.a);
}
`;

interface Renderable {
  vertices: GPUBuffer;
  indices: GPUBuffer;
  indexCount: number;
  bindGroup?: GPUBindGroup;
}

const rng = seedrandom("asteroid");
const randomValues: number[] = [];
for (let j = 0; j < 30000; j++) {
  randomValues.push(rng());
}

describe("Render Bundles", () => {
  it("Asteroid", async () => {
    const result = await client.eval(
      ({
        device,
        gpu,
        ctx,
        meshWGSL,
        mat4,
        vec3,
        assets: { saturn, moon },
        vals,
      }) => {
        interface SphereMesh {
          vertices: Float32Array;
          indices: Uint16Array;
        }

        const SphereLayout = {
          vertexStride: 8 * 4,
          positionsOffset: 0,
          normalOffset: 3 * 4,
          uvOffset: 6 * 4,
        };

        let k = 0;
        const random = () => {
          k++;
          return vals[k % vals.length];
        };

        // Borrowed and simplified from https://github.com/mrdoob/three.js/blob/master/src/geometries/SphereGeometry.js
        function createSphereMesh(
          radius: number,
          widthSegments = 32,
          heightSegments = 16,
          randomness = 0,
        ): SphereMesh {
          const vertices = [];
          const indices = [];

          widthSegments = Math.max(3, Math.floor(widthSegments));
          heightSegments = Math.max(2, Math.floor(heightSegments));

          const firstVertex = vec3.create();
          const vertex = vec3.create();
          const normal = vec3.create();

          let index = 0;
          const grid = [];

          // generate vertices, normals and uvs
          for (let iy = 0; iy <= heightSegments; iy++) {
            const verticesRow = [];
            const v = iy / heightSegments;

            // special case for the poles
            let uOffset = 0;
            if (iy === 0) {
              uOffset = 0.5 / widthSegments;
            } else if (iy === heightSegments) {
              uOffset = -0.5 / widthSegments;
            }

            for (let ix = 0; ix <= widthSegments; ix++) {
              const u = ix / widthSegments;

              // Poles should just use the same position all the way around.
              if (ix === widthSegments) {
                vec3.copy(firstVertex, vertex);
              } else if (ix === 0 || (iy !== 0 && iy !== heightSegments)) {
                const rr = radius + (random() - 0.5) * 2 * randomness * radius;

                // vertex
                vertex[0] =
                  -rr * Math.cos(u * Math.PI * 2) * Math.sin(v * Math.PI);
                vertex[1] = rr * Math.cos(v * Math.PI);
                vertex[2] =
                  rr * Math.sin(u * Math.PI * 2) * Math.sin(v * Math.PI);

                if (ix === 0) {
                  vec3.copy(vertex, firstVertex);
                }
              }

              for (const element of vertex) {
                vertices.push(element);
              }

              // normal
              vec3.copy(vertex, normal);
              vec3.normalize(normal, normal);
              for (const element of normal) {
                vertices.push(element);
              }

              // uv
              vertices.push(u + uOffset, 1 - v);
              verticesRow.push(index++);
            }

            grid.push(verticesRow);
          }

          // indices
          for (let iy = 0; iy < heightSegments; iy++) {
            for (let ix = 0; ix < widthSegments; ix++) {
              const a = grid[iy][ix + 1];
              const b = grid[iy][ix];
              const c = grid[iy + 1][ix];
              const d = grid[iy + 1][ix + 1];

              if (iy !== 0) {
                indices.push(a, b, d);
              }
              if (iy !== heightSegments - 1) {
                indices.push(b, c, d);
              }
            }
          }

          return {
            vertices: new Float32Array(vertices),
            indices: new Uint16Array(indices),
          };
        }

        const useRenderBundles = true;
        const asteroidCount = 5000;
        const presentationFormat = gpu.getPreferredCanvasFormat();

        const shaderModule = device.createShaderModule({
          code: meshWGSL,
        });

        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: shaderModule,
            buffers: [
              {
                arrayStride: SphereLayout.vertexStride,
                attributes: [
                  {
                    // position
                    shaderLocation: 0,
                    offset: SphereLayout.positionsOffset,
                    format: "float32x3",
                  },
                  {
                    // normal
                    shaderLocation: 1,
                    offset: SphereLayout.normalOffset,
                    format: "float32x3",
                  },
                  {
                    // uv
                    shaderLocation: 2,
                    offset: SphereLayout.uvOffset,
                    format: "float32x2",
                  },
                ],
              },
            ],
          },
          fragment: {
            module: shaderModule,
            targets: [
              {
                format: presentationFormat,
              },
            ],
          },
          primitive: {
            topology: "triangle-list",

            // Backface culling since the sphere is solid piece of geometry.
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

        // Fetch the images and upload them into a GPUTexture.
        let planetTexture: GPUTexture;
        {
          //const response = await fetch("../../assets/img/saturn.jpg");
          const imageBitmap = saturn;

          planetTexture = device.createTexture({
            size: [imageBitmap.width, imageBitmap.height, 1],
            format: "rgba8unorm",
            usage:
              GPUTextureUsage.TEXTURE_BINDING |
              GPUTextureUsage.COPY_DST |
              GPUTextureUsage.RENDER_ATTACHMENT,
          });

          device.queue.copyExternalImageToTexture(
            { source: imageBitmap },
            { texture: planetTexture },
            [imageBitmap.width, imageBitmap.height],
          );
        }

        let moonTexture: GPUTexture;
        {
          const imageBitmap = moon;

          moonTexture = device.createTexture({
            size: [imageBitmap.width, imageBitmap.height, 1],
            format: "rgba8unorm",
            usage:
              GPUTextureUsage.TEXTURE_BINDING |
              GPUTextureUsage.COPY_DST |
              GPUTextureUsage.RENDER_ATTACHMENT,
          });

          device.queue.copyExternalImageToTexture(
            { source: imageBitmap },
            { texture: moonTexture },
            [imageBitmap.width, imageBitmap.height],
          );
        }

        const sampler = device.createSampler({
          magFilter: "linear",
          minFilter: "linear",
        });

        // Helper functions to create the required meshes and bind groups for each sphere.
        function createSphereRenderable(
          radius: number,
          widthSegments = 32,
          heightSegments = 16,
          randomness = 0,
        ): Renderable {
          const sphereMesh = createSphereMesh(
            radius,
            widthSegments,
            heightSegments,
            randomness,
          );

          // Create a vertex buffer from the sphere data.
          const vertices = device.createBuffer({
            size: sphereMesh.vertices.byteLength,
            usage: GPUBufferUsage.VERTEX,
            mappedAtCreation: true,
          });
          new Float32Array(vertices.getMappedRange()).set(sphereMesh.vertices);
          vertices.unmap();

          const indices = device.createBuffer({
            size: sphereMesh.indices.byteLength,
            usage: GPUBufferUsage.INDEX,
            mappedAtCreation: true,
          });
          new Uint16Array(indices.getMappedRange()).set(sphereMesh.indices);
          indices.unmap();

          return {
            vertices,
            indices,
            indexCount: sphereMesh.indices.length,
          };
        }

        function createSphereBindGroup(
          texture: GPUTexture,
          transform: Float32Array,
        ): GPUBindGroup {
          // eslint-disable-next-line @typescript-eslint/no-shadow
          const uniformBufferSize = 4 * 16; // 4x4 matrix
          // eslint-disable-next-line @typescript-eslint/no-shadow
          const uniformBuffer = device.createBuffer({
            size: uniformBufferSize,
            usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
            mappedAtCreation: true,
          });
          new Float32Array(uniformBuffer.getMappedRange()).set(transform);
          uniformBuffer.unmap();

          const bindGroup = device.createBindGroup({
            layout: pipeline.getBindGroupLayout(1),
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
                resource: texture.createView(),
              },
            ],
          });

          return bindGroup;
        }

        const transform = mat4.create();
        mat4.identity(transform);

        // Create one large central planet surrounded by a large ring of asteroids
        const planet = createSphereRenderable(1.0);
        planet.bindGroup = createSphereBindGroup(planetTexture, transform);

        const asteroids = [
          createSphereRenderable(0.01, 8, 6, 0.15),
          createSphereRenderable(0.013, 8, 6, 0.15),
          createSphereRenderable(0.017, 8, 6, 0.15),
          createSphereRenderable(0.02, 8, 6, 0.15),
          createSphereRenderable(0.03, 16, 8, 0.15),
        ];

        const renderables = [planet];

        function ensureEnoughAsteroids() {
          for (let i = renderables.length; i <= asteroidCount; ++i) {
            // Place copies of the asteroid in a ring.
            const radius = random() * 1.7 + 1.25;
            const angle = random() * Math.PI * 2;
            const x = Math.sin(angle) * radius;
            const y = (random() - 0.5) * 0.015;
            const z = Math.cos(angle) * radius;

            mat4.identity(transform);
            mat4.translate(transform, [x, y, z], transform);
            mat4.rotateX(transform, random() * Math.PI, transform);
            mat4.rotateY(transform, random() * Math.PI, transform);
            renderables.push({
              ...asteroids[i % asteroids.length],
              bindGroup: createSphereBindGroup(moonTexture, transform),
            });
          }
        }
        ensureEnoughAsteroids();

        const renderPassDescriptor: GPURenderPassDescriptor = {
          // @ts-expect-error
          colorAttachments: [
            {
              view: undefined, // Assigned later

              clearValue: [0, 0, 0, 1],
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

        const frameBindGroup = device.createBindGroup({
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

        function getTransformationMatrix() {
          const viewMatrix = mat4.identity();
          mat4.translate(viewMatrix, vec3.fromValues(0, 0, -4), viewMatrix);
          const now = 1721766068905;
          // Tilt the view matrix so the planet looks like it's off-axis.
          mat4.rotateZ(viewMatrix, Math.PI * 0.1, viewMatrix);
          mat4.rotateX(viewMatrix, Math.PI * 0.1, viewMatrix);
          // Rotate the view matrix slowly so the planet appears to spin.
          mat4.rotateY(viewMatrix, now * 0.05, viewMatrix);

          mat4.multiply(
            projectionMatrix,
            viewMatrix,
            modelViewProjectionMatrix,
          );

          return modelViewProjectionMatrix;
        }

        // Render bundles function as partial, limited render passes, so we can use the
        // same code both to render the scene normally and to build the render bundle.
        function renderScene(
          passEncoder: GPURenderPassEncoder | GPURenderBundleEncoder,
        ) {
          passEncoder.setPipeline(pipeline);
          passEncoder.setBindGroup(0, frameBindGroup);

          // Loop through every renderable object and draw them individually.
          // (Because many of these meshes are repeated, with only the transforms
          // differing, instancing would be highly effective here. This sample
          // intentionally avoids using instancing in order to emulate a more complex
          // scene, which helps demonstrate the potential time savings a render bundle
          // can provide.)
          let count = 0;
          for (const renderable of renderables) {
            passEncoder.setBindGroup(1, renderable.bindGroup!);
            passEncoder.setVertexBuffer(0, renderable.vertices);
            passEncoder.setIndexBuffer(renderable.indices, "uint16");
            passEncoder.drawIndexed(renderable.indexCount);

            if (++count > asteroidCount) {
              break;
            }
          }
        }

        // The render bundle can be encoded once and re-used as many times as needed.
        // Because it encodes all of the commands needed to render at the GPU level,
        // those commands will not need to execute the associated JavaScript code upon
        // execution or be re-validated, which can represent a significant time savings.
        //
        // However, because render bundles are immutable once created, they are only
        // appropriate for rendering content where the same commands will be executed
        // every time, with the only changes being the contents of the buffers and
        // textures used. Cases where the executed commands differ from frame-to-frame,
        // such as when using frustrum or occlusion culling, will not benefit from
        // using render bundles as much.
        let renderBundle: GPURenderBundle;
        function updateRenderBundle() {
          const renderBundleEncoder = device.createRenderBundleEncoder({
            colorFormats: [presentationFormat],
            depthStencilFormat: "depth24plus",
          });
          renderScene(renderBundleEncoder);
          renderBundle = renderBundleEncoder.finish();
        }
        updateRenderBundle();

        function frame() {
          const transformationMatrix = getTransformationMatrix();
          device.queue.writeBuffer(
            uniformBuffer,
            0,
            transformationMatrix.buffer,
            transformationMatrix.byteOffset,
            transformationMatrix.byteLength,
          );
          // @ts-expect-error
          renderPassDescriptor.colorAttachments[0].view = ctx
            .getCurrentTexture()
            .createView();

          const commandEncoder = device.createCommandEncoder();
          const passEncoder =
            commandEncoder.beginRenderPass(renderPassDescriptor);

          if (useRenderBundles) {
            // Executing a bundle is equivalent to calling all of the commands encoded
            // in the render bundle as part of the current render pass.
            passEncoder.executeBundles([renderBundle]);
          } else {
            // Alternatively, the same render commands can be encoded manually, which
            // can take longer since each command needs to be interpreted by the
            // JavaScript virtual machine and re-validated each time.
            renderScene(passEncoder);
          }

          passEncoder.end();
          device.queue.submit([commandEncoder.finish()]);
        }
        frame();
        return ctx.getImageData();
      },
      { meshWGSL: mesh, vals: randomValues },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/asteroid.png", { maxPixelDiff: 500 });
  });
});
