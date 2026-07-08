export const cameraEffectVertWGSL = /* wgsl */ `
@vertex
fn vs_main(@builtin(vertex_index) vi: u32) -> @builtin(position) vec4f {
  let pos = array(
    vec2f(-1.0, -1.0),
    vec2f( 3.0, -1.0),
    vec2f(-1.0,  3.0),
  );
  return vec4f(pos[vi], 0.0, 1.0);
}
`;

export const cameraEffectFragWGSL = /* wgsl */ `
struct Params {
  time: f32,
  aberration: f32,
  vignette: f32,
  width: f32,
  height: f32,
}

@group(0) @binding(0) var<uniform> u: Params;

fn scene(uv: vec2f, time: f32) -> vec3f {
  let p = uv * vec2f(1.2, 1.0) + vec2f(time * 0.08, time * 0.05);
  let face = sin(p.x * 12.0) * sin(p.y * 10.0);
  let skin = vec3f(0.85, 0.65, 0.55);
  let bg = vec3f(0.15, 0.35, 0.55);
  return mix(bg, skin, smoothstep(-0.2, 0.6, face + sin(time + p.x * 3.0) * 0.15));
}

@fragment
fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f {
  let uv = pos.xy / vec2f(u.width, u.height);
  let ab = u.aberration;
  let r = scene(uv + vec2f(ab, 0.0), u.time).r;
  let g = scene(uv, u.time).g;
  let b = scene(uv - vec2f(ab, 0.0), u.time).b;
  var color = vec3f(r, g, b);
  let d = length(uv - vec2f(0.5));
  color *= 1.0 - smoothstep(0.55, 0.95, d) * u.vignette;
  return vec4f(color, 1.0);
}
`;
