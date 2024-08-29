export const meshWGSL = /*wgsl*/ `struct Uniforms {
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

export const vertexPositionColorWGSL = /* wgsl */ `@fragment
fn main(
  @location(0) fragUV: vec2f,
  @location(1) fragPosition: vec4f
) -> @location(0) vec4f {
  return fragPosition;
}
`;

export const basicVertWGSL = /* wgsl */ `struct Uniforms {
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

export const sampleTextureMixColorWGSL = /* wgsl */ `@group(0) @binding(1) var mySampler: sampler;
@group(0) @binding(2) var myTexture: texture_2d<f32>;

@fragment
fn main(
  @location(0) fragUV: vec2f,
  @location(1) fragPosition: vec4f
) -> @location(0) vec4f {
  return textureSample(myTexture, mySampler, fragUV) * fragPosition;
}
`;

export const sampleSelfWGSL = /* wgsl */ `@binding(1) @group(0) var mySampler: sampler;
@binding(2) @group(0) var myTexture: texture_2d<f32>;

@fragment
fn main(
  @location(0) fragUV: vec2f,
  @location(1) fragPosition: vec4f
) -> @location(0) vec4f {
  let texColor = textureSample(myTexture, mySampler, fragUV * 0.8 + vec2(0.1));
  let f = select(1.0, 0.0, length(texColor.rgb - vec3(0.5)) < 0.01);
  return f * texColor + (1.0 - f) * fragPosition;
}`;

export const instancedVertWGSL = /* wgsl */ `struct Uniforms {
  modelViewProjectionMatrix : array<mat4x4f, 16>,
}

@binding(0) @group(0) var<uniform> uniforms : Uniforms;

struct VertexOutput {
  @builtin(position) Position : vec4f,
  @location(0) fragUV : vec2f,
  @location(1) fragPosition: vec4f,
}

@vertex
fn main(
  @builtin(instance_index) instanceIdx : u32,
  @location(0) position : vec4f,
  @location(1) uv : vec2f
) -> VertexOutput {
  var output : VertexOutput;
  output.Position = uniforms.modelViewProjectionMatrix[instanceIdx] * position;
  output.fragUV = uv;
  output.fragPosition = 0.5 * (position + vec4(1.0));
  return output;
}
`;

export const sampleCubemapWGSL = /* wgsl */ `@group(0) @binding(1) var mySampler: sampler;
@group(0) @binding(2) var myTexture: texture_cube<f32>;

@fragment
fn main(
  @location(0) fragUV: vec2f,
  @location(1) fragPosition: vec4f
) -> @location(0) vec4f {
  // Our camera and the skybox cube are both centered at (0, 0, 0)
  // so we can use the cube geometry position to get viewing vector to sample
  // the cube texture. The magnitude of the vector doesn't matter.
  var cubemapVec = fragPosition.xyz - vec3(0.5);
  // When viewed from the inside, cubemaps are left-handed (z away from viewer),
  // but common camera matrix convention results in a right-handed world space
  // (z toward viewer), so we have to flip it.
  cubemapVec.z *= -1;
  return textureSample(myTexture, mySampler, cubemapVec);
}
`;
