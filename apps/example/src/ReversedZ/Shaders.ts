export const fragmentTextureQuadWGSL = /* wgsl */ `@group(0) @binding(0) var depthTexture: texture_depth_2d;

@fragment
fn main(
  @builtin(position) coord : vec4f
) -> @location(0) vec4f {
  let depthValue = textureLoad(depthTexture, vec2i(floor(coord.xy)), 0);
  return vec4f(depthValue, depthValue, depthValue, 1.0);
}
`;

export const vertexTextureQuadWGSL = /* wgsl */ `@vertex
fn main(
  @builtin(vertex_index) VertexIndex : u32
) -> @builtin(position) vec4f {
  const pos = array(
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
    vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
  );

  return vec4(pos[VertexIndex], 0.0, 1.0);
}
`;

export const vertexWGSL = /* wgsl */ `struct Uniforms {
  modelMatrix : array<mat4x4f, 5>,
}
struct Camera {
  viewProjectionMatrix : mat4x4f,
}

@binding(0) @group(0) var<uniform> uniforms : Uniforms;
@binding(1) @group(0) var<uniform> camera : Camera;

struct VertexOutput {
  @builtin(position) Position : vec4f,
  @location(0) fragColor : vec4f,
}

@vertex
fn main(
  @builtin(instance_index) instanceIdx : u32,
  @location(0) position : vec4f,
  @location(1) color : vec4f
) -> VertexOutput {
  var output : VertexOutput;
  output.Position = camera.viewProjectionMatrix * uniforms.modelMatrix[instanceIdx] * position;
  output.fragColor = color;
  return output;
}`;

export const fragmentWGSL = /* wgsl */ `@fragment
fn main(
  @location(0) fragColor: vec4f
) -> @location(0) vec4f {
  return fragColor;
}
`;

export const vertexPrecisionErrorPassWGSL = /* wgsl */ `struct Uniforms {
  modelMatrix : array<mat4x4f, 5>,
}
struct Camera {
  viewProjectionMatrix : mat4x4f,
}

@binding(0) @group(0) var<uniform> uniforms : Uniforms;
@binding(1) @group(0) var<uniform> camera : Camera;

struct VertexOutput {
  @builtin(position) Position : vec4f,
  @location(0) clipPos : vec4f,
}

@vertex
fn main(
  @builtin(instance_index) instanceIdx : u32,
  @location(0) position : vec4f
) -> VertexOutput {
  var output : VertexOutput;
  output.Position = camera.viewProjectionMatrix * uniforms.modelMatrix[instanceIdx] * position;
  output.clipPos = output.Position;
  return output;
}
`;

export const fragmentPrecisionErrorPassWGSL = /* wgsl */ `@group(1) @binding(0) var depthTexture: texture_depth_2d;

@fragment
fn main(
  @builtin(position) coord: vec4f,
  @location(0) clipPos: vec4f
) -> @location(0) vec4f {
  let depthValue = textureLoad(depthTexture, vec2i(floor(coord.xy)), 0);
  let v : f32 = abs(clipPos.z / clipPos.w - depthValue) * 2000000.0;
  return vec4f(v, v, v, 1.0);
}
`;

export const vertexDepthPrePassWGSL = /* wgsl */ `struct Uniforms {
  modelMatrix : array<mat4x4f, 5>,
}
struct Camera {
  viewProjectionMatrix : mat4x4f,
}

@binding(0) @group(0) var<uniform> uniforms : Uniforms;
@binding(1) @group(0) var<uniform> camera : Camera;

@vertex
fn main(
  @builtin(instance_index) instanceIdx : u32,
  @location(0) position : vec4f
) -> @builtin(position) vec4f {
  return camera.viewProjectionMatrix * uniforms.modelMatrix[instanceIdx] * position;
}
`;
