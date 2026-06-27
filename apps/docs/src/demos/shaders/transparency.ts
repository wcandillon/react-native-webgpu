export const transparencyVertWGSL = `@vertex
fn main(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4f {
  var pos = array<vec2f, 6>(
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
  );

  return vec4f(pos[vertexIndex], 0.0, 1.0);
}`;

export const transparencyFragWGSL = `@group(0) @binding(0) var<uniform> params: vec2f;

struct FragmentOutput {
  @location(0) color: vec4f,
};

@fragment
fn main(@builtin(position) position: vec4f) -> FragmentOutput {
  let size = params;
  let uv = position.xy / size;
  let center = vec2(0.5, 0.5);
  let aspect = size.x / max(size.y, 1.0);
  let p = uv - center;
  p.x *= aspect;
  let d = length(p);
  let pulse = 0.5 + 0.5 * sin(params.y * 2.5);
  let radius = mix(0.22, 0.3, pulse);
  let alpha = smoothstep(radius, radius - 0.04, d) * 0.72;
  let rgb = vec3(1.0, 0.35, 0.15);
  var out: FragmentOutput;
  out.color = vec4(rgb * alpha, alpha);
  return out;
}`;
