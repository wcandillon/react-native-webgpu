/* eslint-disable @typescript-eslint/ban-ts-comment */
import { checkImage, client, encodeImage } from "../setup";
import { mesh as teapotMesh } from "../components/meshes/teapot";

const opaque = /*wgsl*/ `struct Uniforms {
  modelViewProjectionMatrix: mat4x4f,
};

@binding(0) @group(0) var<uniform> uniforms: Uniforms;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) instance: u32
};

@vertex
fn main_vs(@location(0) position: vec4f, @builtin(instance_index) instance: u32) -> VertexOutput {
  var output: VertexOutput;

  // distribute instances into a staggered 4x4 grid
  const gridWidth = 125.0;
  const cellSize = gridWidth / 4.0;
  let row = instance / 2u;
  let col = instance % 2u;

  let xOffset = -gridWidth / 2.0 + cellSize / 2.0 + 2.0 * cellSize * f32(col) + f32(row % 2u != 0u) * cellSize;
  let zOffset = -gridWidth / 2.0 + cellSize / 2.0 + 2.0 + f32(row) * cellSize;

  let offsetPos = vec4(position.x + xOffset, position.y, position.z + zOffset, position.w);

  output.position = uniforms.modelViewProjectionMatrix * offsetPos;
  output.instance = instance;
  return output;
}

@fragment
fn main_fs(@location(0) @interpolate(flat) instance: u32) -> @location(0) vec4f {
  const colors = array<vec3f,6>(
      vec3(1.0, 0.0, 0.0),
      vec3(0.0, 1.0, 0.0),
      vec3(0.0, 0.0, 1.0),
      vec3(1.0, 0.0, 1.0),
      vec3(1.0, 1.0, 0.0),
      vec3(0.0, 1.0, 1.0),
  );

  return vec4(colors[instance % 6u], 1.0);
}
`;

const translucent = /*wgsl*/ `struct Uniforms {
  modelViewProjectionMatrix: mat4x4f,
  maxStorableFragments: u32,
  targetWidth: u32,
};

struct SliceInfo {
  sliceStartY: i32
};

struct Heads {
  numFragments: atomic<u32>,
  data: array<atomic<u32>>
};

struct LinkedListElement {
  color: vec4f,
  depth: f32,
  next: u32
};

struct LinkedList {
  data: array<LinkedListElement>
};

@binding(0) @group(0) var<uniform> uniforms: Uniforms;
@binding(1) @group(0) var<storage, read_write> heads: Heads;
@binding(2) @group(0) var<storage, read_write> linkedList: LinkedList;
@binding(3) @group(0) var opaqueDepthTexture: texture_depth_2d;
@binding(4) @group(0) var<uniform> sliceInfo: SliceInfo;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) instance: u32
};

@vertex
fn main_vs(@location(0) position: vec4f, @builtin(instance_index) instance: u32) -> VertexOutput {
  var output: VertexOutput;

  // distribute instances into a staggered 4x4 grid
  const gridWidth = 125.0;
  const cellSize = gridWidth / 4.0;
  let row = instance / 2u;
  let col = instance % 2u;

  let xOffset = -gridWidth / 2.0 + cellSize / 2.0 + 2.0 * cellSize * f32(col) + f32(row % 2u == 0u) * cellSize;
  let zOffset = -gridWidth / 2.0 + cellSize / 2.0 + 2.0 + f32(row) * cellSize;

  let offsetPos = vec4(position.x + xOffset, position.y, position.z + zOffset, position.w);

  output.position = uniforms.modelViewProjectionMatrix * offsetPos;
  output.instance = instance;

  return output;
}

@fragment
fn main_fs(@builtin(position) position: vec4f, @location(0) @interpolate(flat) instance: u32) {
  const colors = array<vec3f,6>(
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0),
    vec3(0.0, 1.0, 1.0),
  );

  let fragCoords = vec2i(position.xy);
  let opaqueDepth = textureLoad(opaqueDepthTexture, fragCoords, 0);

  // reject fragments behind opaque objects
  if position.z >= opaqueDepth {
    discard;
  }

  // The index in the heads buffer corresponding to the head data for the fragment at
  // the current location.
  let headsIndex = u32(fragCoords.y - sliceInfo.sliceStartY) * uniforms.targetWidth + u32(fragCoords.x);

  // The index in the linkedList buffer at which to store the new fragment
  let fragIndex = atomicAdd(&heads.numFragments, 1u);

  // If we run out of space to store the fragments, we just lose them
  if fragIndex < uniforms.maxStorableFragments {
    let lastHead = atomicExchange(&heads.data[headsIndex], fragIndex);
    linkedList.data[fragIndex].depth = position.z;
    linkedList.data[fragIndex].next = lastHead;
    linkedList.data[fragIndex].color = vec4(colors[(instance + 3u) % 6u], 0.3);
  }
}
`;

const composite = /*wgsl*/ `struct Uniforms {
  modelViewProjectionMatrix: mat4x4f,
  maxStorableFragments: u32,
  targetWidth: u32,
};

struct SliceInfo {
  sliceStartY: i32
};

struct Heads {
  numFragments: u32,
  data: array<u32>
};

struct LinkedListElement {
  color: vec4f,
  depth: f32,
  next: u32
};

struct LinkedList {
  data: array<LinkedListElement>
};

@binding(0) @group(0) var<uniform> uniforms: Uniforms;
@binding(1) @group(0) var<storage, read_write> heads: Heads;
@binding(2) @group(0) var<storage, read_write> linkedList: LinkedList;
@binding(3) @group(0) var<uniform> sliceInfo: SliceInfo;

// Output a full screen quad
@vertex
fn main_vs(@builtin(vertex_index) vertIndex: u32) -> @builtin(position) vec4f {
  const position = array<vec2f, 6>(
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(-1.0, 1.0),
  );
  
  return vec4(position[vertIndex], 0.0, 1.0);
}

@fragment
fn main_fs(@builtin(position) position: vec4f) -> @location(0) vec4f {
  let fragCoords = vec2i(position.xy);
  let headsIndex = u32(fragCoords.y - sliceInfo.sliceStartY) * uniforms.targetWidth + u32(fragCoords.x);

  // The maximum layers we can process for any pixel
  const maxLayers = 12u;

  var layers: array<LinkedListElement, maxLayers>;

  var numLayers = 0u;
  var elementIndex = heads.data[headsIndex];

  // copy the list elements into an array up to the maximum amount of layers
  while elementIndex != 0xFFFFFFFFu && numLayers < maxLayers {
    layers[numLayers] = linkedList.data[elementIndex];
    numLayers++;
    elementIndex = linkedList.data[elementIndex].next;
  }

  if numLayers == 0u {
    discard;
  }
  
  // sort the fragments by depth
  for (var i = 1u; i < numLayers; i++) {
    let toInsert = layers[i];
    var j = i;

    while j > 0u && toInsert.depth > layers[j - 1u].depth {
      layers[j] = layers[j - 1u];
      j--;
    }

    layers[j] = toInsert;
  }

  // pre-multiply alpha for the first layer
  var color = vec4(layers[0].color.a * layers[0].color.rgb, layers[0].color.a);

  // blend the remaining layers
  for (var i = 1u; i < numLayers; i++) {
    let mixed = mix(color.rgb, layers[i].color.rgb, layers[i].color.aaa);
    color = vec4(mixed, color.a);
  }

  return color;
}`;

describe("A Buffer", () => {
  it("draw scene", async () => {
    const result = await client.eval(
      ({
        ctx,
        device,
        gpu,
        mesh,
        opaqueWGSL,
        compositeWGSL,
        translucentWGSL,
        mat4,
        vec3,
      }) => {
        const presentationFormat = gpu.getPreferredCanvasFormat();
        const settings = {
          memoryStrategy: "multipass",
        };

        function roundUp(n: number, k: number): number {
          return Math.ceil(n / k) * k;
        }

        // Create the model vertex buffer
        const vertexBuffer = device.createBuffer({
          size: 3 * mesh.positions.length * Float32Array.BYTES_PER_ELEMENT,
          usage: GPUBufferUsage.VERTEX,
          mappedAtCreation: true,
          label: "vertexBuffer",
        });
        {
          const mapping = new Float32Array(vertexBuffer.getMappedRange());
          for (let i = 0; i < mesh.positions.length; ++i) {
            mapping.set(mesh.positions[i], 3 * i);
          }
          vertexBuffer.unmap();
        }

        // Create the model index buffer
        const indexCount = mesh.triangles.length * 3;
        const indexBuffer = device.createBuffer({
          size: indexCount * Uint16Array.BYTES_PER_ELEMENT,
          usage: GPUBufferUsage.INDEX,
          mappedAtCreation: true,
          label: "indexBuffer",
        });
        {
          const mapping = new Uint16Array(indexBuffer.getMappedRange());
          for (let i = 0; i < mesh.triangles.length; ++i) {
            mapping.set(mesh.triangles[i], 3 * i);
          }
          indexBuffer.unmap();
        }

        // Uniforms contains:
        // * modelViewProjectionMatrix: mat4x4f
        // * maxStorableFragments: u32
        // * targetWidth: u32
        const uniformsSize = roundUp(
          16 * Float32Array.BYTES_PER_ELEMENT +
            2 * Uint32Array.BYTES_PER_ELEMENT,
          16,
        );

        const uniformBuffer = device.createBuffer({
          size: uniformsSize,
          usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
          label: "uniformBuffer",
        });

        const opaqueModule = device.createShaderModule({
          code: opaqueWGSL,
          label: "opaqueModule",
        });

        const opaquePipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: opaqueModule,
            buffers: [
              {
                arrayStride: 3 * Float32Array.BYTES_PER_ELEMENT,
                attributes: [
                  {
                    // position
                    format: "float32x3",
                    offset: 0,
                    shaderLocation: 0,
                  },
                ],
              },
            ],
          },
          fragment: {
            module: opaqueModule,
            targets: [
              {
                format: presentationFormat,
              },
            ],
          },
          primitive: {
            topology: "triangle-list",
          },
          depthStencil: {
            depthWriteEnabled: true,
            depthCompare: "less",
            format: "depth24plus",
          },
          label: "opaquePipeline",
        });

        const opaquePassDescriptor: GPURenderPassDescriptor = {
          // @ts-expect-error
          colorAttachments: [
            {
              view: undefined,
              clearValue: [0, 0, 0, 1.0],
              loadOp: "clear",
              storeOp: "store",
            },
          ],
          depthStencilAttachment: {
            // @ts-expect-error
            view: undefined,
            depthClearValue: 1.0,
            depthLoadOp: "clear",
            depthStoreOp: "store",
          },
          label: "opaquePassDescriptor",
        };

        const opaqueBindGroup = device.createBindGroup({
          layout: opaquePipeline.getBindGroupLayout(0),
          entries: [
            {
              binding: 0,
              resource: {
                buffer: uniformBuffer,
                size: 16 * Float32Array.BYTES_PER_ELEMENT,
                label: "modelViewProjection",
              },
            },
          ],
          label: "opaquePipeline",
        });

        const translucentModule = device.createShaderModule({
          code: translucentWGSL,
          label: "translucentModule",
        });

        const translucentBindGroupLayout = device.createBindGroupLayout({
          label: "translucentBindGroupLayout",
          entries: [
            {
              binding: 0,
              visibility: GPUShaderStage.VERTEX | GPUShaderStage.FRAGMENT,
              buffer: {
                type: "uniform",
              },
            },
            {
              binding: 1,
              visibility: GPUShaderStage.FRAGMENT,
              buffer: {
                type: "storage",
              },
            },
            {
              binding: 2,
              visibility: GPUShaderStage.FRAGMENT,
              buffer: {
                type: "storage",
              },
            },
            {
              binding: 3,
              visibility: GPUShaderStage.FRAGMENT,
              texture: { sampleType: "depth" },
            },
            {
              binding: 4,
              visibility: GPUShaderStage.FRAGMENT,
              buffer: {
                type: "uniform",
                hasDynamicOffset: true,
              },
            },
          ],
        });

        const translucentPipeline = device.createRenderPipeline({
          layout: device.createPipelineLayout({
            bindGroupLayouts: [translucentBindGroupLayout],
            label: "translucentPipelineLayout",
          }),
          vertex: {
            module: translucentModule,
            buffers: [
              {
                arrayStride: 3 * Float32Array.BYTES_PER_ELEMENT,
                attributes: [
                  {
                    format: "float32x3",
                    offset: 0,
                    shaderLocation: 0,
                  },
                ],
              },
            ],
          },
          fragment: {
            module: translucentModule,
            targets: [
              {
                format: presentationFormat,
                writeMask: 0x0,
              },
            ],
          },
          primitive: {
            topology: "triangle-list",
          },
          label: "translucentPipeline",
        });

        const translucentPassDescriptor: GPURenderPassDescriptor = {
          // @ts-expect-error
          colorAttachments: [
            {
              loadOp: "load",
              storeOp: "store",
              view: undefined,
            },
          ],
          label: "translucentPassDescriptor",
        };

        const compositeModule = device.createShaderModule({
          code: compositeWGSL,
          label: "compositeModule",
        });

        const compositeBindGroupLayout = device.createBindGroupLayout({
          label: "compositeBindGroupLayout",
          entries: [
            {
              binding: 0,
              visibility: GPUShaderStage.VERTEX | GPUShaderStage.FRAGMENT,
              buffer: {
                type: "uniform",
              },
            },
            {
              binding: 1,
              visibility: GPUShaderStage.FRAGMENT,
              buffer: {
                type: "storage",
              },
            },
            {
              binding: 2,
              visibility: GPUShaderStage.FRAGMENT,
              buffer: {
                type: "storage",
              },
            },
            {
              binding: 3,
              visibility: GPUShaderStage.FRAGMENT,
              buffer: {
                type: "uniform",
                hasDynamicOffset: true,
              },
            },
          ],
        });

        const compositePipeline = device.createRenderPipeline({
          layout: device.createPipelineLayout({
            bindGroupLayouts: [compositeBindGroupLayout],
            label: "compositePipelineLayout",
          }),
          vertex: {
            module: compositeModule,
          },
          fragment: {
            module: compositeModule,
            targets: [
              {
                format: presentationFormat,
                blend: {
                  color: {
                    srcFactor: "one",
                    operation: "add",
                    dstFactor: "one-minus-src-alpha",
                  },
                  alpha: {},
                },
              },
            ],
          },
          primitive: {
            topology: "triangle-list",
          },
          label: "compositePipeline",
        });

        const compositePassDescriptor: GPURenderPassDescriptor = {
          // @ts-expect-error
          colorAttachments: [
            {
              view: undefined,
              loadOp: "load",
              storeOp: "store",
            },
          ],
          label: "compositePassDescriptor",
        };

        const configure = () => {
          // The default maximum storage buffer binding size is 128Mib. The amount
          // of memory we need to store transparent fragments depends on the size
          // of the canvas and the average number of layers per fragment we want to
          // support. When the devicePixelRatio is 1, we know that 128Mib is enough
          // to store 4 layers per pixel at 600x600. However, when the device pixel
          // ratio is high enough we will exceed this limit.
          //
          // We provide 2 choices of mitigations to this issue:
          // 1) Clamp the device pixel ratio to a value which we know will not break
          //    the limit. The tradeoff here is that the canvas resolution will not
          //    match the native resolution and therefore may have a reduction in
          //    quality.
          // 2) Break the frame into a series of horizontal slices using the scissor
          //    functionality and process a single slice at a time. This limits memory
          //    usage because we only need enough memory to process the dimensions
          //    of the slice. The tradeoff is the performance reduction due to multiple
          //    passes.
          if (settings.memoryStrategy === "clamp-pixel-ratio") {
            devicePixelRatio = 1;
          }

          const depthTexture = device.createTexture({
            size: [ctx.width, ctx.height],
            format: "depth24plus",
            usage:
              GPUTextureUsage.RENDER_ATTACHMENT |
              GPUTextureUsage.TEXTURE_BINDING,
            label: "depthTexture",
          });

          const depthTextureView = depthTexture.createView({
            label: "depthTextureView",
          });

          // Determines how much memory is allocated to store linked-list elements
          const averageLayersPerFragment = 4;

          // Each element stores
          // * color : vec4f
          // * depth : f32
          // * index of next element in the list : u32
          const linkedListElementSize =
            5 * Float32Array.BYTES_PER_ELEMENT +
            1 * Uint32Array.BYTES_PER_ELEMENT;

          // We want to keep the linked-list buffer size under the maxStorageBufferBindingSize.
          // Split the frame into enough slices to meet that constraint.
          const bytesPerline =
            ctx.width * averageLayersPerFragment * linkedListElementSize;
          const maxLinesSupported = Math.floor(
            device.limits.maxStorageBufferBindingSize / bytesPerline,
          );
          const numSlices = Math.ceil(ctx.height / maxLinesSupported);
          const sliceHeight = Math.ceil(ctx.height / numSlices);
          const linkedListBufferSize = sliceHeight * bytesPerline;

          const linkedListBuffer = device.createBuffer({
            size: linkedListBufferSize,
            usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
            label: "linkedListBuffer",
          });

          // To slice up the frame we need to pass the starting fragment y position of the slice.
          // We do this using a uniform buffer with a dynamic offset.
          const sliceInfoBuffer = device.createBuffer({
            size: numSlices * device.limits.minUniformBufferOffsetAlignment,
            usage: GPUBufferUsage.UNIFORM,
            mappedAtCreation: true,
            label: "sliceInfoBuffer",
          });
          {
            const mapping = new Int32Array(sliceInfoBuffer.getMappedRange());

            // This assumes minUniformBufferOffsetAlignment is a multiple of 4
            const stride =
              device.limits.minUniformBufferOffsetAlignment /
              Int32Array.BYTES_PER_ELEMENT;
            for (let i = 0; i < numSlices; ++i) {
              mapping[i * stride] = i * sliceHeight;
            }
            sliceInfoBuffer.unmap();
          }

          // `Heads` struct contains the start index of the linked-list of translucent fragments
          // for a given pixel.
          // * numFragments : u32
          // * data : array<u32>
          const headsBuffer = device.createBuffer({
            size: (1 + ctx.width * sliceHeight) * Uint32Array.BYTES_PER_ELEMENT,
            usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
            label: "headsBuffer",
          });

          const headsInitBuffer = device.createBuffer({
            size: (1 + ctx.width * sliceHeight) * Uint32Array.BYTES_PER_ELEMENT,
            usage: GPUBufferUsage.COPY_SRC,
            mappedAtCreation: true,
            label: "headsInitBuffer",
          });
          {
            const buffer = new Uint32Array(headsInitBuffer.getMappedRange());

            for (let i = 0; i < buffer.length; ++i) {
              buffer[i] = 0xffffffff;
            }

            headsInitBuffer.unmap();
          }

          const translucentBindGroup = device.createBindGroup({
            layout: translucentBindGroupLayout,
            entries: [
              {
                binding: 0,
                resource: {
                  buffer: uniformBuffer,
                  label: "uniforms",
                },
              },
              {
                binding: 1,
                resource: {
                  buffer: headsBuffer,
                  label: "headsBuffer",
                },
              },
              {
                binding: 2,
                resource: {
                  buffer: linkedListBuffer,
                  label: "linkedListBuffer",
                },
              },
              {
                binding: 3,
                resource: depthTextureView,
              },
              {
                binding: 4,
                resource: {
                  buffer: sliceInfoBuffer,
                  size: device.limits.minUniformBufferOffsetAlignment,
                  label: "sliceInfoBuffer",
                },
              },
            ],
            label: "translucentBindGroup",
          });

          const compositeBindGroup = device.createBindGroup({
            layout: compositePipeline.getBindGroupLayout(0),
            entries: [
              {
                binding: 0,
                resource: {
                  buffer: uniformBuffer,
                  label: "uniforms",
                },
              },
              {
                binding: 1,
                resource: {
                  buffer: headsBuffer,
                  label: "headsBuffer",
                },
              },
              {
                binding: 2,
                resource: {
                  buffer: linkedListBuffer,
                  label: "linkedListBuffer",
                },
              },
              {
                binding: 3,
                resource: {
                  buffer: sliceInfoBuffer,
                  size: device.limits.minUniformBufferOffsetAlignment,
                  label: "sliceInfoBuffer",
                },
              },
            ],
          });
          // @ts-expect-error
          opaquePassDescriptor.depthStencilAttachment.view = depthTextureView;

          // Rotates the camera around the origin based on time.
          function getCameraViewProjMatrix() {
            const aspect = ctx.width / ctx.height;

            const projectionMatrix = mat4.perspective(
              (2 * Math.PI) / 5,
              aspect,
              1,
              2000.0,
            );

            const upVector = vec3.fromValues(0, 1, 0);
            const origin = vec3.fromValues(0, 0, 0);
            const eyePosition = vec3.fromValues(0, 5, -100);

            const now = 1721824271091;
            const rad = Math.PI * (now / 5000);
            const rotation = mat4.rotateY(mat4.translation(origin), rad);
            vec3.transformMat4(eyePosition, rotation, eyePosition);

            const viewMatrix = mat4.lookAt(eyePosition, origin, upVector);

            const viewProjMatrix = mat4.multiply(projectionMatrix, viewMatrix);
            return viewProjMatrix;
          }

          return function doDraw() {
            // update the uniform buffer
            {
              const buffer = new ArrayBuffer(uniformBuffer.size);

              new Float32Array(buffer).set(getCameraViewProjMatrix());
              new Uint32Array(buffer, 16 * Float32Array.BYTES_PER_ELEMENT).set([
                averageLayersPerFragment * ctx.width * sliceHeight,
                ctx.width,
              ]);

              device.queue.writeBuffer(uniformBuffer, 0, buffer);
            }

            const commandEncoder = device.createCommandEncoder();
            const textureView = ctx.getCurrentTexture().createView();

            // Draw the opaque objects
            // @ts-expect-error
            opaquePassDescriptor.colorAttachments[0].view = textureView;
            const opaquePassEncoder =
              commandEncoder.beginRenderPass(opaquePassDescriptor);
            opaquePassEncoder.setPipeline(opaquePipeline);
            opaquePassEncoder.setBindGroup(0, opaqueBindGroup);
            opaquePassEncoder.setVertexBuffer(0, vertexBuffer);
            opaquePassEncoder.setIndexBuffer(indexBuffer, "uint16");
            opaquePassEncoder.drawIndexed(mesh.triangles.length * 3, 8);
            opaquePassEncoder.end();

            for (let slice = 0; slice < numSlices; ++slice) {
              // initialize the heads buffer
              commandEncoder.copyBufferToBuffer(
                headsInitBuffer,
                0,
                headsBuffer,
                0,
                headsInitBuffer.size,
              );

              const scissorX = 0;
              const scissorY = slice * sliceHeight;
              const scissorWidth = ctx.width;
              const scissorHeight =
                Math.min((slice + 1) * sliceHeight, ctx.height) -
                slice * sliceHeight;

              // Draw the translucent objects
              // @ts-expect-error
              translucentPassDescriptor.colorAttachments[0].view = textureView;
              const translucentPassEncoder = commandEncoder.beginRenderPass(
                translucentPassDescriptor,
              );

              // Set the scissor to only process a horizontal slice of the frame
              translucentPassEncoder.setScissorRect(
                scissorX,
                scissorY,
                scissorWidth,
                scissorHeight,
              );

              translucentPassEncoder.setPipeline(translucentPipeline);
              translucentPassEncoder.setBindGroup(0, translucentBindGroup, [
                slice * device.limits.minUniformBufferOffsetAlignment,
              ]);
              translucentPassEncoder.setVertexBuffer(0, vertexBuffer);
              translucentPassEncoder.setIndexBuffer(indexBuffer, "uint16");
              translucentPassEncoder.drawIndexed(mesh.triangles.length * 3, 8);
              translucentPassEncoder.end();

              // Composite the opaque and translucent objects
              // @ts-expect-error
              compositePassDescriptor.colorAttachments[0].view = textureView;
              const compositePassEncoder = commandEncoder.beginRenderPass(
                compositePassDescriptor,
              );

              // Set the scissor to only process a horizontal slice of the frame
              compositePassEncoder.setScissorRect(
                scissorX,
                scissorY,
                scissorWidth,
                scissorHeight,
              );

              compositePassEncoder.setPipeline(compositePipeline);
              compositePassEncoder.setBindGroup(0, compositeBindGroup, [
                slice * device.limits.minUniformBufferOffsetAlignment,
              ]);
              compositePassEncoder.draw(6);
              compositePassEncoder.end();
            }

            device.queue.submit([commandEncoder.finish()]);
          };
        };

        const doDraw = configure();

        doDraw();

        return ctx.getImageData();
      },
      {
        mesh: teapotMesh,
        opaqueWGSL: opaque,
        translucentWGSL: translucent,
        compositeWGSL: composite,
      },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/abuffer.png");
  });
});
