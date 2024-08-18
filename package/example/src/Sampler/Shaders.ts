export const texturedSquareWGSL = /* wgsl */ `struct Config {
  viewProj: mat4x4f,
  animationOffset: vec2f,
  flangeSize: f32,
  highlightFlange: f32,
};
@group(0) @binding(0) var<uniform> config: Config;
@group(0) @binding(1) var<storage, read> matrices: array<mat4x4f>;
@group(0) @binding(2) var samp: sampler;
@group(0) @binding(3) var tex: texture_2d<f32>;

struct Varying {
  @builtin(position) pos: vec4f,
  @location(0) uv: vec2f,
}

override kTextureBaseSize: f32;
override kViewportSize: f32;

@vertex
fn vmain(
  @builtin(instance_index) instance_index: u32,
  @builtin(vertex_index) vertex_index: u32,
) -> Varying {
  let flange = config.flangeSize;
  var uvs = array(
    vec2(-flange, -flange), vec2(-flange, 1 + flange), vec2(1 + flange, -flange),
    vec2(1 + flange, -flange), vec2(-flange, 1 + flange), vec2(1 + flange, 1 + flange),
  );
  // Default size (if matrix is the identity) makes 1 texel = 1 pixel.
  let radius = (1 + 2 * flange) * kTextureBaseSize / kViewportSize;
  var positions = array(
    vec2(-radius, -radius), vec2(-radius, radius), vec2(radius, -radius),
    vec2(radius, -radius), vec2(-radius, radius), vec2(radius, radius),
  );

  let modelMatrix = matrices[instance_index];
  let pos = config.viewProj * modelMatrix * vec4f(positions[vertex_index] + config.animationOffset, 0, 1);
  return Varying(pos, uvs[vertex_index]);
}

@fragment
fn fmain(vary: Varying) -> @location(0) vec4f {
  let uv = vary.uv;
  var color = textureSample(tex, samp, uv);

  let outOfBounds = uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1;
  if config.highlightFlange > 0 && outOfBounds {
    color += vec4(0.7, 0, 0, 0);
  }

  return color;
}
`;

export const showTextureWGSL = /* wgsl */ `@group(0) @binding(0) var tex: texture_2d<f32>;

struct Varying {
  @builtin(position) pos: vec4f,
  @location(0) texelCoord: vec2f,
  @location(1) mipLevel: f32,
}

const kMipLevels = 4;
const baseMipSize: u32 = 16;

@vertex
fn vmain(
  @builtin(instance_index) instance_index: u32, // used as mipLevel
  @builtin(vertex_index) vertex_index: u32,
) -> Varying {
  var square = array(
    vec2f(0, 0), vec2f(0, 1), vec2f(1, 0),
    vec2f(1, 0), vec2f(0, 1), vec2f(1, 1),
  );
  let uv = square[vertex_index];
  let pos = vec4(uv * 2 - vec2(1, 1), 0.0, 1.0);

  let mipLevel = instance_index;
  let mipSize = f32(1 << (kMipLevels - mipLevel));
  let texelCoord = uv * mipSize;
  return Varying(pos, texelCoord, f32(mipLevel));
}

@fragment
fn fmain(vary: Varying) -> @location(0) vec4f {
  return textureLoad(tex, vec2u(vary.texelCoord), u32(vary.mipLevel));
}
`;
