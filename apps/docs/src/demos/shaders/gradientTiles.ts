const gradientTilesVertWGSL = /* wgsl */ `
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

const gradientTilesFragWGSL = /* wgsl */ `
struct Uniforms {
  span: vec2f,
}

@group(0) @binding(0) var<uniform> u: Uniforms;

@fragment
fn fs_main(@builtin(position) pos: vec4f) -> @location(0) vec4f {
  let uv = pos.xy / vec2f(800.0, 600.0);
  let red = floor(uv.x * u.span.x) / u.span.x;
  let green = floor(uv.y * u.span.y) / u.span.y;
  return vec4f(red, green, 0.5, 1.0);
}
`;

export { gradientTilesFragWGSL, gradientTilesVertWGSL };
