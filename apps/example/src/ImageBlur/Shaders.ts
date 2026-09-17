// Adapted from the official WebGPU image blur sample:
// https://github.com/webgpu/webgpu-samples/tree/main/sample/imageBlur
export const BLUR_SHADER = /* wgsl */ `
struct Params {
  filterDim: i32,
  blockDim: u32,
}

@group(0) @binding(0) var samp: sampler;
@group(0) @binding(1) var<uniform> params: Params;
@group(1) @binding(1) var inputTex: texture_2d<f32>;
@group(1) @binding(2) var outputTex: texture_storage_2d<rgba8unorm, write>;

struct Flip {
  value: u32,
}
@group(1) @binding(3) var<uniform> flip: Flip;

var<workgroup> tile: array<array<vec3f, 128>, 4>;

@compute @workgroup_size(32, 1, 1)
fn main(
  @builtin(workgroup_id) workgroupId: vec3u,
  @builtin(local_invocation_id) localInvocationId: vec3u,
) {
  let filterOffset = (params.filterDim - 1) / 2;
  let dimensions = vec2i(textureDimensions(inputTex, 0));
  let baseIndex = vec2i(
    workgroupId.xy * vec2(params.blockDim, 4u) +
    localInvocationId.xy * vec2(4u, 1u)
  ) - vec2(filterOffset, 0);

  for (var row = 0; row < 4; row++) {
    for (var column = 0; column < 4; column++) {
      var loadIndex = baseIndex + vec2(column, row);
      if (flip.value != 0u) {
        loadIndex = loadIndex.yx;
      }
      tile[row][4u * localInvocationId.x + u32(column)] = textureSampleLevel(
        inputTex,
        samp,
        (vec2f(loadIndex) + vec2f(0.5)) / vec2f(dimensions),
        0.0
      ).rgb;
    }
  }

  workgroupBarrier();

  for (var row = 0; row < 4; row++) {
    for (var column = 0; column < 4; column++) {
      var writeIndex = baseIndex + vec2(column, row);
      if (flip.value != 0u) {
        writeIndex = writeIndex.yx;
      }
      let center = i32(4u * localInvocationId.x) + column;
      if (
        center >= filterOffset &&
        center < 128 - filterOffset &&
        all(writeIndex < dimensions)
      ) {
        var color = vec3f(0.0);
        for (var index = 0; index < params.filterDim; index++) {
          let tileIndex = center + index - filterOffset;
          color += tile[row][tileIndex] / f32(params.filterDim);
        }
        textureStore(outputTex, writeIndex, vec4f(color, 1.0));
      }
    }
  }
}
`;

export const FULLSCREEN_TEXTURE_SHADER = /* wgsl */ `
@group(0) @binding(0) var imageSampler: sampler;
@group(0) @binding(1) var imageTexture: texture_2d<f32>;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
}

@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
  const positions = array(
    vec2f( 1.0,  1.0),
    vec2f( 1.0, -1.0),
    vec2f(-1.0, -1.0),
    vec2f( 1.0,  1.0),
    vec2f(-1.0, -1.0),
    vec2f(-1.0,  1.0),
  );
  const uvs = array(
    vec2f(1.0, 0.0),
    vec2f(1.0, 1.0),
    vec2f(0.0, 1.0),
    vec2f(1.0, 0.0),
    vec2f(0.0, 1.0),
    vec2f(0.0, 0.0),
  );

  var output: VertexOutput;
  output.position = vec4f(positions[vertexIndex], 0.0, 1.0);
  output.uv = uvs[vertexIndex];
  return output;
}

@fragment
fn fragmentMain(@location(0) uv: vec2f) -> @location(0) vec4f {
  return textureSample(imageTexture, imageSampler, uv);
}
`;
