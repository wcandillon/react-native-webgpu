// Shared WGSL building blocks for the hero shader gallery.
//
// Every shader in the registry is written against this small contract so that
// porting a new one (from an author or from compute.toys) only requires writing
// the body, not the plumbing. The standard uniforms live at @group(0)
// @binding(0); compute shaders additionally get a write-only `screen` storage
// texture at @group(0) @binding(1). Shaders that need extra resources (textures,
// samplers, extra uniforms) declare them in @group(1) via a `resources` factory.

// std140-ish layout, 32 bytes:
//   resolution: vec2f   @ 0
//   time:       f32     @ 8
//   theme:      f32     @ 12   (0 = dark, 1 = light)
//   frame:      f32     @ 16
//   dpr:        f32     @ 20
//   (padding to 32)
export const HERO_UNIFORM_SIZE = 32;

export const UNIFORM_STRUCT = /* wgsl */ `
struct HeroUniforms {
  resolution: vec2f,
  time: f32,
  theme: f32,
  frame: f32,
  dpr: f32,
};
@group(0) @binding(0) var<uniform> u: HeroUniforms;
`;

// Fullscreen triangle. Shared by every fragment shader and by the compute blit.
export const FULLSCREEN_VERTEX = /* wgsl */ `
@vertex
fn vs_main(@builtin(vertex_index) vi: u32) -> @builtin(position) vec4f {
  var pos = array(
    vec2f(-1.0, -1.0),
    vec2f(3.0, -1.0),
    vec2f(-1.0, 3.0),
  );
  return vec4f(pos[vi], 0.0, 1.0);
}
`;

// Storage texture compute shaders write into. Matches compute.toys' `screen`.
export const COMPUTE_SCREEN_BINDING = /* wgsl */ `
@group(0) @binding(1) var screen: texture_storage_2d<rgba16float, write>;
`;

export const SCREEN_FORMAT: GPUTextureFormat = "rgba16float";

// Blit pass: samples the compute output texture onto the swapchain. Uses a uv
// varying from the vertex stage so it upscales correctly when the source texture
// is smaller than the render target (compute shaders may render at a capped
// internal resolution).
export const BLIT_SHADER = /* wgsl */ `
@group(0) @binding(0) var src: texture_2d<f32>;
@group(0) @binding(1) var srcSampler: sampler;

struct BlitOut {
  @builtin(position) pos: vec4f,
  @location(0) uv: vec2f,
};

@vertex
fn vs_main(@builtin(vertex_index) vi: u32) -> BlitOut {
  var p = array(
    vec2f(-1.0, -1.0),
    vec2f(3.0, -1.0),
    vec2f(-1.0, 3.0),
  );
  var out: BlitOut;
  let xy = p[vi];
  out.pos = vec4f(xy, 0.0, 1.0);
  out.uv = vec2f(0.5 * xy.x + 0.5, 0.5 - 0.5 * xy.y);
  return out;
}

@fragment
fn fs_main(in: BlitOut) -> @location(0) vec4f {
  return textureSample(src, srcSampler, in.uv);
}
`;
