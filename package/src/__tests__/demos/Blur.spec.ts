import { checkImage, client, encodeImage } from "../setup";

const blur = /*wgsl*/ `struct Params {
  filterDim : i32,
  blockDim : u32,
}

@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var<uniform> params : Params;
@group(1) @binding(1) var inputTex : texture_2d<f32>;
@group(1) @binding(2) var outputTex : texture_storage_2d<rgba8unorm, write>;

struct Flip {
  value : u32,
}
@group(1) @binding(3) var<uniform> flip : Flip;

// This shader blurs the input texture in one direction, depending on whether
// |flip.value| is 0 or 1.
// It does so by running (128 / 4) threads per workgroup to load 128
// texels into 4 rows of shared memory. Each thread loads a
// 4 x 4 block of texels to take advantage of the texture sampling
// hardware.
// Then, each thread computes the blur result by averaging the adjacent texel values
// in shared memory.
// Because we're operating on a subset of the texture, we cannot compute all of the
// results since not all of the neighbors are available in shared memory.
// Specifically, with 128 x 128 tiles, we can only compute and write out
// square blocks of size 128 - (filterSize - 1). We compute the number of blocks
// needed in Javascript and dispatch that amount.

var<workgroup> tile : array<array<vec3f, 128>, 4>;

@compute @workgroup_size(32, 1, 1)
fn main(
  @builtin(workgroup_id) WorkGroupID : vec3u,
  @builtin(local_invocation_id) LocalInvocationID : vec3u
) {
  let filterOffset = (params.filterDim - 1) / 2;
  let dims = vec2i(textureDimensions(inputTex, 0));
  let baseIndex = vec2i(WorkGroupID.xy * vec2(params.blockDim, 4) +
                            LocalInvocationID.xy * vec2(4, 1))
                  - vec2(filterOffset, 0);

  for (var r = 0; r < 4; r++) {
    for (var c = 0; c < 4; c++) {
      var loadIndex = baseIndex + vec2(c, r);
      if (flip.value != 0u) {
        loadIndex = loadIndex.yx;
      }

      tile[r][4 * LocalInvocationID.x + u32(c)] = textureSampleLevel(
        inputTex,
        samp,
        (vec2f(loadIndex) + vec2f(0.25, 0.25)) / vec2f(dims),
        0.0
      ).rgb;
    }
  }

  workgroupBarrier();

  for (var r = 0; r < 4; r++) {
    for (var c = 0; c < 4; c++) {
      var writeIndex = baseIndex + vec2(c, r);
      if (flip.value != 0) {
        writeIndex = writeIndex.yx;
      }

      let center = i32(4 * LocalInvocationID.x) + c;
      if (center >= filterOffset &&
          center < 128 - filterOffset &&
          all(writeIndex < dims)) {
        var acc = vec3(0.0, 0.0, 0.0);
        for (var f = 0; f < params.filterDim; f++) {
          var i = center + f - filterOffset;
          acc = acc + (1.0 / f32(params.filterDim)) * tile[r][i];
        }
        textureStore(outputTex, writeIndex, vec4(acc, 1.0));
      }
    }
  }
}
`;

const fullscreenTexturedQuad = /*wgsl*/ `@group(0) @binding(0) var mySampler : sampler;
@group(0) @binding(1) var myTexture : texture_2d<f32>;

struct VertexOutput {
  @builtin(position) Position : vec4f,
  @location(0) fragUV : vec2f,
}

@vertex
fn vert_main(@builtin(vertex_index) VertexIndex : u32) -> VertexOutput {
  const pos = array(
    vec2( 1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2(-1.0,  1.0),
  );

  const uv = array(
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(0.0, 0.0),
  );

  var output : VertexOutput;
  output.Position = vec4(pos[VertexIndex], 0.0, 1.0);
  output.fragUV = uv[VertexIndex];
  return output;
}

@fragment
fn frag_main(@location(0) fragUV : vec2f) -> @location(0) vec4f {
  return textureSample(myTexture, mySampler, fragUV);
}
`;

describe("Blur", () => {
  it("draw scene", async () => {
    const result = await client.eval(
      ({
        ctx,
        device,
        gpu,
        assets: { di3D: imageBitmap },
        fullscreenTexturedQuadWGSL,
        blurWGSL,
      }) => {
        const tileDim = 128;
        const batch = [4, 4];
        const presentationFormat = gpu.getPreferredCanvasFormat();
        const blurPipeline = device.createComputePipeline({
          layout: "auto",
          compute: {
            module: device.createShaderModule({
              code: blurWGSL,
            }),
          },
        });

        const fullscreenQuadPipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: device.createShaderModule({
              code: fullscreenTexturedQuadWGSL,
            }),
          },
          fragment: {
            module: device.createShaderModule({
              code: fullscreenTexturedQuadWGSL,
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
        });

        const sampler = device.createSampler({
          magFilter: "linear",
          minFilter: "linear",
        });

        const [srcWidth, srcHeight] = [imageBitmap.width, imageBitmap.height];
        const cubeTexture = device.createTexture({
          size: [srcWidth, srcHeight, 1],
          format: "rgba8unorm",
          usage:
            GPUTextureUsage.TEXTURE_BINDING |
            GPUTextureUsage.COPY_DST |
            GPUTextureUsage.RENDER_ATTACHMENT,
        });
        device.queue.writeTexture(
          { texture: cubeTexture, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
          imageBitmap.data.buffer,
          {
            offset: 0,
            bytesPerRow: 4 * imageBitmap.width,
            rowsPerImage: imageBitmap.height,
          },
          { width: imageBitmap.width, height: imageBitmap.height },
        );
        const textures = [0, 1].map(() => {
          return device.createTexture({
            size: {
              width: srcWidth,
              height: srcHeight,
            },
            format: "rgba8unorm",
            usage:
              GPUTextureUsage.COPY_DST |
              GPUTextureUsage.STORAGE_BINDING |
              GPUTextureUsage.TEXTURE_BINDING,
          });
        });

        const buffer0 = (() => {
          const buffer = device.createBuffer({
            size: 4,
            mappedAtCreation: true,
            usage: GPUBufferUsage.UNIFORM,
          });
          new Uint32Array(buffer.getMappedRange())[0] = 0;
          buffer.unmap();
          return buffer;
        })();

        const buffer1 = (() => {
          const buffer = device.createBuffer({
            size: 4,
            mappedAtCreation: true,
            usage: GPUBufferUsage.UNIFORM,
          });
          new Uint32Array(buffer.getMappedRange())[0] = 1;
          buffer.unmap();
          return buffer;
        })();

        const blurParamsBuffer = device.createBuffer({
          size: 8,
          usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM,
        });

        const computeConstants = device.createBindGroup({
          layout: blurPipeline.getBindGroupLayout(0),
          entries: [
            {
              binding: 0,
              resource: sampler,
            },
            {
              binding: 1,
              resource: {
                buffer: blurParamsBuffer,
              },
            },
          ],
        });

        const computeBindGroup0 = device.createBindGroup({
          layout: blurPipeline.getBindGroupLayout(1),
          entries: [
            {
              binding: 1,
              resource: cubeTexture.createView(),
            },
            {
              binding: 2,
              resource: textures[0].createView(),
            },
            {
              binding: 3,
              resource: {
                buffer: buffer0,
              },
            },
          ],
        });

        const computeBindGroup1 = device.createBindGroup({
          layout: blurPipeline.getBindGroupLayout(1),
          entries: [
            {
              binding: 1,
              resource: textures[0].createView(),
            },
            {
              binding: 2,
              resource: textures[1].createView(),
            },
            {
              binding: 3,
              resource: {
                buffer: buffer1,
              },
            },
          ],
        });

        const computeBindGroup2 = device.createBindGroup({
          layout: blurPipeline.getBindGroupLayout(1),
          entries: [
            {
              binding: 1,
              resource: textures[1].createView(),
            },
            {
              binding: 2,
              resource: textures[0].createView(),
            },
            {
              binding: 3,
              resource: {
                buffer: buffer0,
              },
            },
          ],
        });

        const showResultBindGroup = device.createBindGroup({
          layout: fullscreenQuadPipeline.getBindGroupLayout(0),
          entries: [
            {
              binding: 0,
              resource: sampler,
            },
            {
              binding: 1,
              resource: textures[1].createView(),
            },
          ],
        });

        const settings = {
          filterSize: 15,
          iterations: 2,
        };

        let blockDim: number;
        const updateSettings = () => {
          blockDim = tileDim - (settings.filterSize - 1);
          device.queue.writeBuffer(
            blurParamsBuffer,
            0,
            new Uint32Array([settings.filterSize, blockDim]),
          );
        };
        updateSettings();

        function frame() {
          const commandEncoder = device.createCommandEncoder();

          const computePass = commandEncoder.beginComputePass();
          computePass.setPipeline(blurPipeline);
          computePass.setBindGroup(0, computeConstants);

          computePass.setBindGroup(1, computeBindGroup0);
          computePass.dispatchWorkgroups(
            Math.ceil(srcWidth / blockDim),
            Math.ceil(srcHeight / batch[1]),
          );

          computePass.setBindGroup(1, computeBindGroup1);
          computePass.dispatchWorkgroups(
            Math.ceil(srcHeight / blockDim),
            Math.ceil(srcWidth / batch[1]),
          );

          for (let i = 0; i < settings.iterations - 1; ++i) {
            computePass.setBindGroup(1, computeBindGroup2);
            computePass.dispatchWorkgroups(
              Math.ceil(srcWidth / blockDim),
              Math.ceil(srcHeight / batch[1]),
            );

            computePass.setBindGroup(1, computeBindGroup1);
            computePass.dispatchWorkgroups(
              Math.ceil(srcHeight / blockDim),
              Math.ceil(srcWidth / batch[1]),
            );
          }

          computePass.end();

          const passEncoder = commandEncoder.beginRenderPass({
            colorAttachments: [
              {
                view: ctx.getCurrentTexture().createView(),
                clearValue: [0, 0, 0, 1],
                loadOp: "clear",
                storeOp: "store",
              },
            ],
          });

          passEncoder.setPipeline(fullscreenQuadPipeline);
          passEncoder.setBindGroup(0, showResultBindGroup);
          passEncoder.draw(6);
          passEncoder.end();
          device.queue.submit([commandEncoder.finish()]);
        }
        frame();
        return ctx.getImageData();
      },
      { blurWGSL: blur, fullscreenTexturedQuadWGSL: fullscreenTexturedQuad },
    );
    const image = encodeImage(result);
    checkImage(image, "snapshots/blur.png");
  });
});
